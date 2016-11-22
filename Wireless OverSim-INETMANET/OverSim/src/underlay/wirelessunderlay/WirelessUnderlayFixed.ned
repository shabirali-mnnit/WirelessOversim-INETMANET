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

package oversim.underlay.wirelessunderlay;

import inet.networklayer.autorouting.ipv4.HostAutoConfigurator2;
import inet.networklayer.autorouting.ipv4.IPv4NetworkConfigurator;
import inet.nodes.inet.AdhocHost;
import inet.world.radio.IdealChannelModel;
import oversim.common.GlobalObserver;
import oversim.common.GlobalStatistics;

import oversim.common.ChurnGenerator;
import oversim.common.UnderlayConfigurator;


//
// The SimpleUnderlay ned-file @see SimpleUnderlayConfigurator
//
// @author Stephan Krause, Bernhard Heep
//
network WirelessUnderlayNetworkFixed
{
    parameters:
       double nodeSeparation @unit(m) = default(30m);
              //@display("bgb=4000,4000"); // uncomment for large canvas
    submodules:
        underlayConfigurator: WirelessUnderlayConfigurator {
            @display("p=70,130");
        }
        churnGenerator[0]: ChurnGenerator {
            @display("p=70,210,column");
        }
        globalObserver: GlobalObserver {
            @display("p=70,50");
        }
        overlayTerminal[0]: WirelessOverlayHostAdHocManet {
                mobilityType = "LinearNodeDistributionMobility";
                mobility.initialY = 315m;// (floor(index/sqrt(numFixHosts))*nodeSeparation)+(nodeSeparation);
                mobility.initialX = (nodeSeparation/2.0)+nodeSeparation;
                mobility.separation = nodeSeparation*2;
            @dynamic;
        }
        channelControl: IdealChannelModel {
            @display("p=150,60");
        }
}
