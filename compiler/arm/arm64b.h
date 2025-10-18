
// arm64b.h

//-----------------------------------------------------------------

/// processLogicalImmediate - Determine if an immediate value can be encoded
/// as the immediate operand of a logical instruction for the given register
/// size.  If so, return true with "encoding" set to the encoded value in
/// the form N:immr:imms (13 bit)

bool processLogicalImmediate (    int8 Im,
                                  uint RegSize,   // 32 or 64
                              out uint Encoding);

//-----------------------------------------------------------------
