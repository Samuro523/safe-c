
// netcard.h : retrieve list of network cards (Windows-specific)

use win/windows;

// list must be freed by caller after use.
void get_list_of_network_cards (out NETCARD[]^ list);
