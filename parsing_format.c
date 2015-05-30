/* Parsing Format */

/* For the Control Message sent between servers, 
 * send them in string, seperated by commas, in the sequence:
 *   "Type,NodeID,CostForA,CostForB,...,CostForF"
 * For type, 'C' for control, 'D' for data. So here we should use 'C'.
 * For NodeID, we use integers to represent, 0 stands for A, 1 stands for B ... 5 for F;
 * CostForA and CostForB ... and CostForF would be an integer representing the link cost
 	from node indicated by NodeID to the other nodes,and they must be in the order from A to F,
 	and if the cost is unknown yet, use 9999 to represent infinity.
 * eg: "C,0,0,3,5,9999,2,5"
 * meaning: it is a control message from node A, and the costs from A to A,B,...F are
 	0,3,5,infinity,2,5, respectively.
 */

/* For the Datagram
 * send them in string, seperated by commas, in the sequence:
 *    "Type,SourceNodeID,DestinationNodeID,LengthOfData,Data"
 * For type we should use 'D' here.
 * SourceNodeID and DestinationNodeID are represented by integers too.
 * LengthOfData is the number of bytes of the Data
 * eg: "D,2,88,somedatastringhere..."
 * Be aware that Data part might also has comma
 */

/* we need not only parse the data from such format,
 * but also dump the data into such format.
 */