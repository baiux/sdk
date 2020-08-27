/**
 * (c) 2020 by Mega Limited, Wellsford, New Zealand
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

#pragma once

#include <mega/db.h>

#include "NotImplemented.h"

namespace mt {

class DefaultedDbTable: public mega::DbTable
{
public:
    using mega::DbTable::DbTable;
    void rewind() override
    {
        throw NotImplemented{__func__};
    }
    bool next(uint32_t*, std::string*) override
    {
        throw NotImplemented{__func__};
    }
    bool get(uint32_t, std::string*) override
    {
        throw NotImplemented{__func__};
    }
    bool getNode(mega::handle, std::string&) override
    {
        throw NotImplemented{__func__};
    }
    bool getNodes(std::vector<std::string>&) override
    {
        throw NotImplemented{__func__};
    }
    bool getNodesByFingerprint(const mega::FileFingerprint&, std::map<mega::handle, std::string>&) override
    {
        throw NotImplemented{__func__};
    }
    bool getNodeByFingerprint(const mega::FileFingerprint&, std::string&) override
    {
        throw NotImplemented{__func__};
    }
    bool getNodesWithoutParent(std::vector<std::string>&) override
    {
        throw NotImplemented(__func__);
    }
    bool getChildrenFromNode(mega::handle, std::map<mega::handle, std::string>&) override
    {
        throw NotImplemented(__func__);
    }
    bool getChildrenHandlesFromNode(mega::handle, std::vector<mega::handle>&) override
    {
        throw NotImplemented(__func__);
    }
    mega::NodeCounter getNodeCounter(mega::handle) override
    {
        throw NotImplemented(__func__);
    }
    uint32_t getNumberOfChildrenFromNode(mega::handle) override
    {
        throw NotImplemented(__func__);
    }
    bool isNodesOnDemandDb() override
    {
        throw NotImplemented{__func__};
    }
    mega::handle getFirstAncestor(mega::handle) override
    {
        throw NotImplemented{__func__};
    }
    bool put(uint32_t, char*, unsigned) override
    {
        throw NotImplemented{__func__};
    }
    bool put(mega::Node *) override
    {
        throw NotImplemented{__func__};
    }
    bool del(uint32_t) override
    {
        throw NotImplemented{__func__};
    }
    bool del(mega::handle) override
    {
        throw NotImplemented{__func__};
    }
    bool removeNodes() override
    {
        throw NotImplemented{__func__};
    }
    void truncate() override
    {
        throw NotImplemented{__func__};
    }
    void begin() override
    {
        throw NotImplemented{__func__};
    }
    void commit() override
    {
        throw NotImplemented{__func__};
    }
    void abort() override
    {
        throw NotImplemented{__func__};
    }
    void remove() override
    {
        throw NotImplemented{__func__};
    }
};

} // mt
