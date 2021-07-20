We have developed a wireless underlay configurator for simulating structured P2P protocols over wireless networks. For this purpose, the existing Simple Underlay model has the following limitations for simulating wireless underlay network -
1. It has no support to incorporate mobility models for the overlay nodes.
2. Implementing ad hoc routing at network layer does not influence the physical underlay of the overlay network.
3. Does not have capability to use different MAC layer interfaces and radio technologies.
4. The transmission range of a device could not be customized in the model.

These limitations of the Simple Underlay model has tempted us to add support for wireless underlying networks by providing generic interfaces for communication with TCP/IP modules of the INETMANET framework.

We have created a generic interface to bypass the functionalities of SimpleUDP and SimpleTCP of the Simple Underlay model to pass the events to the protocol stack of INETMANET framework.
• We have made provision for assigning IP addresses to overlay nodes dynamically (earlier, static IP addresses were being used by the Simple Underlay model)
• The create() and killpeer() methods of Simple Underlay model of OverSim, that manages joining and leaving of overlay nodes (churn) are modified to incorporate dynamic IP address assignment to nodes and handling of events.

In this modified configurator module, we have the liberty to select any of the available radio models, MAC models, mobility models, ad hoc routing protocols and TCP variants as per our requirement. The detailed description is as available in the following article:


S. Ali, A. Sewak, M. Pandey and N. Tyagi, "Simulation of P2P overlays over MANETs: Impediments and proposed solution," 2017 9th International Conference on Communication Systems and Networks (COMSNETS), 2017, pp. 338-345, doi: 10.1109/COMSNETS.2017.7945395.
