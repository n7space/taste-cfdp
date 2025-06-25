#include "mapping_functions.h"

asn1SccSint cfdp_entity_len_encode(asn1SccSint val) { return val - 1; }

asn1SccSint cfdp_entity_len_decode(asn1SccSint val) { return val + 1; }