/**
 * @file request.cpp
 * @brief Generic request interface
 *
 * (c) 2013-2014 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */

#include "mega/request.h"
#include "mega/command.h"
#include "mega/logging.h"
#include "mega/megaclient.h"

namespace mega {

bool Request::isFetchNodes() const
{
    return cmds.size() == 1 && dynamic_cast<CommandFetchNodes*>(cmds.back());
}

void Request::add(Command* c)
{
    cmds.push_back(c);
}

size_t Request::size() const
{
    return cmds.size();
}

void Request::get(string* req, bool& suppressSID) const
{
    // concatenate all command objects, resulting in an API request
    *req = "[";

    for (int i = 0; i < (int)cmds.size(); i++)
    {
        req->append(i ? ",{" : "{");
        req->append(cmds[i]->getstring());
        req->append("}");
        suppressSID = suppressSID && cmds[i]->suppressSID;
    }

    req->append("]");
}

bool Request::processCmdJSON(Command* cmd)
{
    if (cmd->client->json.enterobject())
    {
        if (!cmd->callProcResult(Command::CmdObject) || !cmd->client->json.leaveobject())
        {
            LOG_err << "Invalid object";
            return false;
        }
    }
    else if (cmd->client->json.enterarray())
    {
        if (!cmd->callProcResult(Command::CmdArray) || !cmd->client->json.leavearray())
        {
            LOG_err << "Invalid array";
            return false;
        }
    }
    else
    {
        return cmd->callProcResult(Command::CmdItem);
    }
    return true;
}

bool Request::processSeqTag(Command* cmd, bool withJSON, bool& parsedOk)
{
    string st;
    cmd->client->json.storeobject(&st);
                
    if (cmd->client->mCurrentSeqtag == st)
    {
        cmd->client->mCurrentSeqtag.clear();
        cmd->client->mCurrentSeqtagSeen = false;
        parsedOk = withJSON ? processCmdJSON(cmd)
                            : cmd->callProcResult(Command::CmdActionpacket);
        return true;
    }
    else
    {
        // result processing paused until we encounter and process actionpackets matching client->mCurrentSeqtag
        cmd->client->mCurrentSeqtag = st;
        cmd->client->mCurrentSeqtagSeen = false;
        cmd->client->mCurrentSeqtagCmdtag = cmd->tag;
        cmd->client->json.pos = nullptr;
        return false;  
    }
}

void Request::process(MegaClient* client)
{
    DBTableTransactionCommitter committer(client->tctable);
    client->mTctableRequestCommitter = &committer;

    client->json = json;
    for (; processindex < cmds.size() && !stopProcessing; processindex++)
    {
        Command* cmd = cmds[processindex];

        client->restag = cmd->tag;

        cmd->client = client;

        auto cmdJSON = client->json;
        bool parsedOk = true;

        Error e;
        if (cmd->checkError(e, client->json))
        {
            if (!cmd->mV3)
            {
                client->json = cmdJSON;
            }
            parsedOk = cmd->callProcResult(Command::CmdError, e);
        }
        else
        {
            if (*client->json.pos == ',') ++client->json.pos;

            if (cmd->mSeqtagArray && client->json.enterarray())
            {
                // Some commands need to return not just a seqtag but also some JSON, in which case they are in an array
                // These can also return 0 instead of a seqtag instead if no actionpacket was produced.  Coding for any error in that field
                assert(cmd->mV3);
                if (client->json.isnumeric())
                {
                    if (error e = error(client->json.getint()))
                    {
                        cmd->callProcResult(Command::CmdError, e);
                        parsedOk = false; // skip any extra json delivered in the array
                    }
                    else
                    {
                        parsedOk = processCmdJSON(cmd);
                    }
                }
                else if (!processSeqTag(cmd, true, parsedOk))
                {
                    // we need to wait for sc processing to catch up with the seqtag we just read
                    json = cmdJSON;
                    return;
                }

                if (parsedOk && !client->json.leavearray())
                {
                    LOG_err << "Invalid seqtag array";
                    parsedOk = false;
                }
            }
            else if (mV3 && !cmd->mStringIsNotSeqtag && *client->json.pos == '"')
            {
                // For v3 commands, a string result is a seqtag.  
                // Except for commands with mStringIsNotSeqtag which already returned a string and don't produce actionpackets.
                if (!processSeqTag(cmd, false, parsedOk))
                {
                    // we need to wait for sc processing to catch up with the seqtag we just read
                    json = cmdJSON;
                    return;
                }
            }
            else
            {
                // straightforward case - plain JSON response, no seqtag, no error
                parsedOk = processCmdJSON(cmd);
            }
        }

        if (!parsedOk)
        {
            LOG_err << "JSON for that command was not recognised/consumed properly, adjusting";
            client->json = cmdJSON;
            client->json.storeobject();
        }
        else
        {
#ifdef DEBUG
            // double check the command consumed the right amount of JSON
            cmdJSON.storeobject();
            assert(client->json.pos == cmdJSON.pos);
#endif
        }
    }

    json = client->json;
    client->json.pos = nullptr;
    if (processindex == cmds.size() || stopProcessing)
    {
        clear();
    }
}

Command* Request::getCurrentCommand()
{
    assert(processindex < cmds.size());
    return cmds[processindex];
}

void Request::serverresponse(std::string&& movestring, MegaClient* client)
{
    assert(processindex == 0);
    jsonresponse = std::move(movestring);
    json.begin(jsonresponse.c_str());

    if (!json.enterarray())
    {
        LOG_err << "Invalid response from server";
    }
}

void Request::servererror(error e, MegaClient* client)
{
    ostringstream s;
    s << "[";
    for (size_t i = cmds.size(); i--; )
    {
        s << e << (i ? "," : "");
    }
    s << "]";
    serverresponse(s.str(), client);
}

void Request::clear()
{
    for (int i = (int)cmds.size(); i--; )
    {
        if (!cmds[i]->persistent)
        {
            delete cmds[i];
        }
    }
    cmds.clear();
    jsonresponse.clear();
    json.pos = NULL;
    processindex = 0;
    stopProcessing = false;
}

bool Request::empty() const
{
    return cmds.empty();
}

void Request::swap(Request& r)
{
    // we use swap to move between queues, but process only after it gets into the completedreqs
    cmds.swap(r.cmds);
    assert(jsonresponse.empty() && r.jsonresponse.empty());
    assert(json.pos == NULL && r.json.pos == NULL);
    assert(processindex == 0 && r.processindex == 0);
    std::swap(mV3, r.mV3);
}

RequestDispatcher::RequestDispatcher()
{
    nextreqs.push_back(Request());
}

#ifdef MEGA_MEASURE_CODE
void RequestDispatcher::sendDeferred()
{
    if (!nextreqs.back().empty())
    {
        nextreqs.push_back(Request());
    }
    nextreqs.back().swap(deferredRequests);
}
#endif

void RequestDispatcher::add(Command *c)
{
#ifdef MEGA_MEASURE_CODE
    if (deferRequests && deferRequests(c))
    {
        deferredRequests.add(c);
        return;
    }
#endif

    if (nextreqs.back().size() >= MAX_COMMANDS)
    {
        LOG_debug << "Starting an additional Request due to MAX_COMMANDS";
        nextreqs.push_back(Request());
    }
    if (c->batchSeparately && !nextreqs.back().empty())
    {
        LOG_debug << "Starting an additional Request for a batch-separately command";
        nextreqs.push_back(Request());
    }

    if (!nextreqs.back().empty() && nextreqs.back().mV3 != c->mV3)
    {
        LOG_debug << "Starting an additional Request for v3 transition " << c->mV3;
        nextreqs.push_back(Request());
    }
    if (nextreqs.back().empty())
    {
        nextreqs.back().mV3 = c->mV3;
    }

    nextreqs.back().add(c);
    if (c->batchSeparately)
    {
        nextreqs.push_back(Request());
    }
}

bool RequestDispatcher::cmdspending() const
{
    return !nextreqs.front().empty();
}

bool RequestDispatcher::cmdsInflight() const
{
    return !inflightreq.empty();
}

Command* RequestDispatcher::getCurrentCommand(bool currSeqtagSeen)
{
    return currSeqtagSeen ? inflightreq.getCurrentCommand() : nullptr;
}

void RequestDispatcher::serverrequest(string *out, bool& suppressSID, bool &includesFetchingNodes, bool& v3)
{
    assert(inflightreq.empty());
    inflightreq.swap(nextreqs.front());
    nextreqs.pop_front();
    if (nextreqs.empty())
    {
        nextreqs.push_back(Request());
    }
    inflightreq.get(out, suppressSID);
    includesFetchingNodes = inflightreq.isFetchNodes();
    v3 = inflightreq.mV3;
#ifdef MEGA_MEASURE_CODE
    csRequestsSent += inflightreq.size();
    csBatchesSent += 1;
#endif
}

void RequestDispatcher::requeuerequest()
{
#ifdef MEGA_MEASURE_CODE
    csBatchesReceived += 1;
#endif
    assert(!inflightreq.empty());
    if (!nextreqs.front().empty())
    {
        nextreqs.push_front(Request());
    }
    nextreqs.front().swap(inflightreq);
}

void RequestDispatcher::serverresponse(std::string&& movestring, MegaClient *client)
{
    CodeCounter::ScopeTimer ccst(client->performanceStats.csResponseProcessingTime);

#ifdef MEGA_MEASURE_CODE
    csBatchesReceived += 1;
    csRequestsCompleted += inflightreq.size();
#endif
    processing = true;
    inflightreq.serverresponse(std::move(movestring), client);
    inflightreq.process(client);
    processing = false;
    if (clearWhenSafe)
    {
        clear();
    }
}

void RequestDispatcher::servererror(error e, MegaClient *client)
{
    // notify all the commands in the batch of the failure
    // so that they can deallocate memory, take corrective action etc.
    processing = true;
    inflightreq.servererror(e, client);
    inflightreq.process(client);
    assert(inflightreq.empty());
    processing = false;
    if (clearWhenSafe)
    {
        clear();
    }
}

void RequestDispatcher::continueProcessing(MegaClient* client)
{
    assert(!inflightreq.empty());
    processing = true;
    inflightreq.process(client);
    processing = false;
}

void RequestDispatcher::clear()
{
    if (processing)
    {
        // we are being called from a command that is in progress (eg. logout) - delay wiping the data structure until that call ends.
        clearWhenSafe = true;
        inflightreq.stopProcessing = true;
    }
    else
    {
        inflightreq.clear();
        for (auto& r : nextreqs)
        {
            r.clear();
        }
        nextreqs.clear();
        nextreqs.push_back(Request());
        processing = false;
        clearWhenSafe = false;
    }
}

} // namespace
