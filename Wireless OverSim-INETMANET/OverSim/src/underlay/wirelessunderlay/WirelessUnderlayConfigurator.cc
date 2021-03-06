//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//

/**
 * @file SimpleUnderlayConfigurator.cc
 * @author Stephan Krause
 * @author Bernhard Heep (migrateRandomNode)
 */

#include <omnetpp.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if !defined(__APPLE__) &&  !defined(_WIN32) && !defined(__ANDROID__)
#include <malloc.h>
#endif

#include <vector>
#include <map>

#include <fstream>

#include <NodeHandle.h>
#include "IInterfaceTable.h"
#include "InterfaceEntry.h"
#include "IPv4InterfaceData.h"
#include "IPv6InterfaceData.h"
#include "TransportAddress.h"
#include "IPvXAddressResolver.h"
#include <cenvir.h>
#include <cxmlelement.h>
#include "ChurnGenerator.h"
#include "GlobalNodeList.h"
#include <StringConvert.h>
#include "IPv4Address.h"

#include "WirelessUnderlayConfigurator.h"

Define_Module(WirelessUnderlayConfigurator);

using namespace std;

WirelessUnderlayConfigurator::~WirelessUnderlayConfigurator()
{
    for (uint32_t i = 0; i < nodeRecordPool.size(); ++i) {
        //std::cout << (nodeRecordPool[i].second ? "+" : "_");
        delete nodeRecordPool[i].first;
    }
    nodeRecordPool.clear();
}

void WirelessUnderlayConfigurator::initializeUnderlay(int stage)
{
    if (stage != MAX_STAGE_UNDERLAY)
        return;

    // fetch some parameters
    fixedNodePositions = par("fixedNodePositions");
    useIPv6 = par("useIPv6Addresses");

    // set maximum coordinates and send queue length
    fieldSize = par("fieldSize");
    sendQueueLength = par("sendQueueLength");

    // get parameter of sourcefile's name
    nodeCoordinateSource = par("nodeCoordinateSource");

    if (std::string(nodeCoordinateSource) != "") {
        // check if exists and process xml-file containing coordinates
        std::ifstream check_for_xml_file(nodeCoordinateSource);
        if (check_for_xml_file) {
            useXmlCoords = 1;

            EV<< "[SimpleNetConfigurator::initializeUnderlay()]\n"
            << "    Using '" << nodeCoordinateSource
            << "' as coordinate source file" << endl;

            maxCoordinate = parseCoordFile(nodeCoordinateSource);
        } else {
            throw cRuntimeError("Coordinate source file not found!");
        }
        check_for_xml_file.close();
    } else {
        useXmlCoords = 0;
        dimensions = 2; //TODO do we need this variable?
        NodeRecord::setDim(dimensions);
        EV << "[SimpleNetConfigurator::initializeUnderlay()]\n"
        << "    Using conventional (random) coordinates for placing nodes!\n"
        << "    (no XML coordinate source file was specified)" << endl;
    }

    // FIXME get address from parameter
    nextFreeAddress = 0x1000001;

    // count the overlay clients
    overlayTerminalCount = 0;

    numCreated = 0;
    numKilled = 0;
}

TransportAddress* WirelessUnderlayConfigurator::createNode(NodeType type,
                                                         bool initialize)
{
    Enter_Method_Silent();
    // derive overlay node from ned
    cModuleType* moduleType = cModuleType::get(type.terminalType.c_str());

    std::string nameStr = "overlayTerminal";
    if (churnGenerator.size() > 1) {
        nameStr += "-" + convertToString<int32_t>(type.typeID);
    }
    cModule* node = moduleType->create(nameStr.c_str(), getParentModule(),
                                       numCreated + 1, numCreated);

    std::string displayString;

    if ((type.typeID > 0) && (type.typeID <= NUM_COLORS)) {
        ((displayString += "i=device/wifilaptop_l,")
                += colorNames[type.typeID - 1]) += ",40;i2=block/circle_s";
    } else {
        displayString = "i=device/wifilaptop_l;i2=block/circle_s";
    }

    node->finalizeParameters();
    node->setDisplayString(displayString.c_str());
    node->buildInside();
    node->scheduleStart(simTime());

    for (int i = 0; i < MAX_STAGE_UNDERLAY + 1; i++) {
        node->callInitialize(i);
    }

   IPv4Address addr=IPvXAddressResolver().addressOf(node).get4();
   nextFreeAddress = addr.getInt();
   SimpleInfo* info = new SimpleInfo(type.typeID, node->getId(), type.context);

    //add node to bootstrap oracle
    globalNodeList->addPeer(addr, info);

    // if the node was not created during startup we have to
    // finish the initialization process manually
    if (!initialize) {
        for (int i = MAX_STAGE_UNDERLAY + 1; i < NUM_STAGES_ALL; i++) {
            node->callInitialize(i);
        }
    }

    //show ip...
    //TODO: migrate
    if (fixedNodePositions && ev.isGUI()) {
        node->getDisplayString().insertTag("p");
        node->getDisplayString().insertTag("t", 0);
        node->getDisplayString().setTagArg("t", 0, addr.str().c_str());
        node->getDisplayString().setTagArg("t", 1, "l");
    }

    overlayTerminalCount++;
    numCreated++;

    churnGenerator[type.typeID]->terminalCount++;

    TransportAddress *address = new TransportAddress(addr);

    // update display
    setDisplayString();

    return address;
}

uint32_t WirelessUnderlayConfigurator::parseCoordFile(const char* nodeCoordinateSource)
{
    cXMLElement* rootElement = ev.getXMLDocument(nodeCoordinateSource);

    // get number of dimensions from attribute of xml rootelement
    dimensions = atoi(rootElement->getAttribute("dimensions"));
    NodeRecord::setDim(dimensions);
    EV << "[SimpleNetConfigurator::parseCoordFile()]\n"
       << "    using " << dimensions << " dimensions: ";

    double max_coord = 0;

    for (cXMLElement *tmpElement = rootElement->getFirstChild(); tmpElement;
         tmpElement = tmpElement->getNextSibling()) {

        // get "ip" and "isRoot" from Attributes (not needed yet)
      /*
       const char* str_ip = tmpElement->getAttribute("ip");
       int tmpIP = 0;
       if (str_ip) sscanf(str_ip, "%x", &tmpIP);
       bool tmpIsRoot = atoi(tmpElement->getAttribute("isroot"));
       */

        // create tmpNode to be added to vector
        NodeRecord* tmpNode = new NodeRecord;

        // get coords from childEntries and fill tmpNodes Array
        int i = 0;
        for (cXMLElement *coord = tmpElement->getFirstChild(); coord;
             coord = coord->getNextSibling()) {

            tmpNode->coords[i] = atof(coord->getNodeValue());

            double newMax = fabs(tmpNode->coords[i]);
            if (newMax > max_coord) {
               max_coord = newMax;
            }
            i++;
        }

        // add to vector
        nodeRecordPool.push_back(make_pair(tmpNode, true));

        //if (nodeRecordPool.size() >= maxSize) break; //TODO use other xml lib
    }

    EV << nodeRecordPool.size()
       << " nodes added to vector \"nodeRecordPool\"." << endl;

#if OMNETPP_VERSION>=0x0401
    // free memory used for xml document
    ev.forgetXMLDocument(nodeCoordinateSource);
#if !defined(__APPLE__) &&  !defined(_WIN32) && !defined(__ANDROID__)
    malloc_trim(0);
#endif
#endif

    return (uint32_t)ceil(max_coord);
}

void WirelessUnderlayConfigurator::preKillNode(NodeType type, TransportAddress* addr)
{
    Enter_Method_Silent();

    //SimpleNodeEntry* entry = NULL;
    SimpleInfo* info;

    if (addr == NULL) {
        addr = globalNodeList->getRandomAliveNode(type.typeID);

        if (addr == NULL) {
            // all nodes are already prekilled
            std::cout << "all nodes are already prekilled" << std::endl;
            return;
        }
    }

    info = dynamic_cast<SimpleInfo*> (globalNodeList->getPeerInfo(*addr));

    if (info != NULL) {
        //entry = info->getEntry();
        globalNodeList->setPreKilled(*addr);
    } else {
        opp_error("SimpleNetConfigurator: Trying to pre kill node "
                  "with nonexistant TransportAddress!");
    }

    uint32_t effectiveType = info->getTypeID();
    cGate* gate;

    cModule* node = gate->getOwnerModule()->getParentModule();

    if (scheduledID.count(node->getId())) {
        std::cout << "SchedID" << std::endl;
        return;
    }

    // remove node from bootstrap oracle
    globalNodeList->removePeer(IPvXAddressResolver().addressOf(node));

    // put node into the kill list and schedule a message for final removal
    // of the node
    killList.push_front(IPvXAddressResolver().addressOf(node));
    scheduledID.insert(node->getId());

    overlayTerminalCount--;
    numKilled++;

    churnGenerator[effectiveType]->terminalCount--;

    // update display
    setDisplayString();

    // inform the notification board about the removal
    NotificationBoard* nb = check_and_cast<NotificationBoard*> (
                             node-> getSubmodule("notificationBoard"));
    nb->fireChangeNotification(NF_OVERLAY_NODE_LEAVE);

    double random = uniform(0, 1);
    if (random < gracefulLeaveProbability) {
        nb->fireChangeNotification(NF_OVERLAY_NODE_GRACEFUL_LEAVE);
    }

    cMessage* msg = new cMessage();
    scheduleAt(simTime() + gracefulLeaveDelay, msg);
}

void WirelessUnderlayConfigurator::migrateNode(NodeType type, TransportAddress* addr)
{
    Enter_Method_Silent();

    //SimpleNodeEntry* entry = NULL;

    if (addr != NULL) {
        SimpleInfo* info =
              dynamic_cast<SimpleInfo*> (globalNodeList->getPeerInfo(*addr));
        if (info != NULL) {
           // entry = info->getEntry();
        } else {
            opp_error("SimpleNetConfigurator: Trying to migrate node with "
                      "nonexistant TransportAddress!");
        }
    } else {
        SimpleInfo* info = dynamic_cast<SimpleInfo*> (
                globalNodeList-> getRandomPeerInfo(type.typeID));
      //  entry = info->getEntry();
    }

    cGate* gate;
    cModule* node = gate->getOwnerModule()->getParentModule();

    // do not migrate node that is already scheduled
    if (scheduledID.count(node->getId()))
        return;

    //std::cout << "migration @ " << tmp_ip << " --> " << address << std::endl;

    // FIXME use only IPv4?
    IPvXAddress address = IPv4Address(nextFreeAddress++);
    IPvXAddress tmp_ip = IPvXAddressResolver().addressOf(node);


    node->bubble("I am migrating!");

    //remove node from bootstrap oracle
    globalNodeList->killPeer(tmp_ip);

    // create meta information
    SimpleInfo* newinfo = new SimpleInfo(type.typeID, node->getId(),
                                         type.context);

    //add node to bootstrap oracle
    globalNodeList->addPeer(address, newinfo);

    // inform the notification board about the migration
    NotificationBoard* nb = check_and_cast<NotificationBoard*> (
                                      node->getSubmodule("notificationBoard"));
    nb->fireChangeNotification(NF_OVERLAY_TRANSPORTADDRESS_CHANGED);
}

void WirelessUnderlayConfigurator::handleTimerEvent(cMessage* msg)
{
    Enter_Method_Silent();

    // get next scheduled node and remove it from the kill list
    IPvXAddress addr = killList.back();
    killList.pop_back();

    //SimpleNodeEntry* entry = NULL;

    SimpleInfo* info =
            dynamic_cast<SimpleInfo*> (globalNodeList->getPeerInfo(addr));

    if (info != NULL) {
      //  entry = info->getEntry();
    } else {
        throw cRuntimeError("SimpleUnderlayConfigurator: Trying to kill "
                            "node with unknown TransportAddress!");
    }

    cGate* gate;
    cModule* node = gate->getOwnerModule()->getParentModule();


    scheduledID.erase(node->getId());
    globalNodeList->killPeer(addr);

    InterfaceEntry* ie = IPvXAddressResolver().interfaceTableOf(node)->getInterfaceByName("wlan");
    delete ie->ipv4Data();

    node->callFinish();
    node->deleteModule();

    delete msg;
}

void WirelessUnderlayConfigurator::setDisplayString()
{
    // Updates the statistics display string.
    char buf[80];
    sprintf(buf, "%i overlay terminals", overlayTerminalCount);
    getDisplayString().setTagArg("t", 0, buf);
}

void WirelessUnderlayConfigurator::finishUnderlay()
{
    // statistics
    recordScalar("Terminals added", numCreated);
    recordScalar("Terminals removed", numKilled);

    if (!isInInitPhase()) {
        struct timeval now, diff;
        gettimeofday(&now, NULL);
        timersub(&now, &initFinishedTime, &diff);
        printf("Simulation time: %li.%06li\n", diff.tv_sec, diff.tv_usec);
    }
}

// new functions for behaviour generator:

IPvXAddress WirelessUnderlayConfigurator::migrateNode(NodeType type, IPvXAddress addr,
                                                    const BaseLocation& locID)
{
    Enter_Method_Silent();

   // SimpleNodeEntry* entry = NULL;

        SimpleInfo* info =
              dynamic_cast<SimpleInfo*> (globalNodeList->getPeerInfo(addr));
        if (info != NULL) {
     //       entry = info->getEntry();
        } else {
            opp_error("SimpleNetConfigurator: Trying to migrate node with "
                      "nonexistent TransportAddress!");
        }

    //const NodeRecord* location = dynamic_cast<const NodeRecord*>(&locID);
    //entry->setX(location->coords[0]);
    //entry->setY(location->coords[1]);

    cGate* gate;
    cModule* node = gate->getOwnerModule()->getParentModule();

    // do not migrate node that is already scheduled
    if (scheduledID.count(node->getId()))
        return addr;

    // FIXME use only IPv4?
    IPvXAddress address = IPv4Address(nextFreeAddress++);
       EV << addr << " migrates to ";
       EV << address << "!" << endl;

    node->bubble("I am migrating!");

    //remove node from bootstrap oracle
    NodeHandle* peer = globalNodeList->getNodeHandle(addr);
    NodeHandle registrationPeer;

    if (peer != NULL) {
        registrationPeer = *peer;
    }

    globalNodeList->killPeer(addr);


    InterfaceEntry* ie = IPvXAddressResolver().interfaceTableOf(node)->getInterfaceByName("wlan");
    delete ie->ipv4Data();

        // create meta information
    SimpleInfo* newinfo = new SimpleInfo(type.typeID, node->getId(),
                                         type.context);
    //newinfo->setEntry(newentry);

    //add node to bootstrap oracle
    globalNodeList->addPeer(address, newinfo);

    if (peer != NULL){
        registrationPeer.setIp(address);
        globalNodeList->registerPeer(registrationPeer);
    }

//    check_and_cast<>BaseRpc>(simulation.getModule(newinfo->getModuleID())->getSubmodule("tier1"));
    // inform the notification board about the migration
    NotificationBoard* nb = check_and_cast<NotificationBoard*> (
                                      node->getSubmodule("notificationBoard"));
    nb->fireChangeNotification(NF_OVERLAY_TRANSPORTADDRESS_CHANGED);

    return address;
}

double WirelessUnderlayConfigurator::getDistance(const BaseLocation& IDa, const BaseLocation& IDb){
    NodeRecord a = dynamic_cast<const NodeRecord&>(IDa);
    NodeRecord b = dynamic_cast<const NodeRecord&>(IDb);
    return sqrt( pow((a.coords[0] - b.coords[0]), 2)
                 + pow((a.coords[1] - b.coords[1]), 2) );
}

BaseLocation* WirelessUnderlayConfigurator::getNearLocation(const BaseLocation& ID, double radius){
    const NodeRecord* tempLoc = dynamic_cast<const NodeRecord*>(&ID);
    NodeRecord* ret = new NodeRecord();

    radius = uniform (0.3, 1, 0) * radius;  // scales the radius randomly
    double angle = uniform (0, 2 *M_PI, 0);  // random angle

    ret->coords[0] = cos(angle)*radius + tempLoc->coords[0];
    ret->coords[1] = sin(angle)*radius + tempLoc->coords[1];
    return ret;
}

BaseLocation* WirelessUnderlayConfigurator::getLocation(IPvXAddress addr){
    SimpleInfo* info = dynamic_cast<SimpleInfo*> (globalNodeList->getPeerInfo(addr));
    BaseLocation* ret = info->getEntry()->getNodeRecord();
    return ret;
}
