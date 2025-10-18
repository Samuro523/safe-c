
// super.h

use config, dbstruct;

//----------------------------------------------------------------------------
#begin unsafe
//----------------------------------------------------------------------------

/* advised_block: if possible, get a block just after 'advised_block'. */
/* alignment    : if possible, get a block from a range having         */
/*                at least 'alignment' entries.                        */

int allocate_block (out LINK    block_nr,
                    ref DB_INFO p,
                        LINK    advised_block,
                        WORD    alignment);

//----------------------------------------------------------------------------

int deallocate_block (LINK block_nr, ref DB_INFO p);

//----------------------------------------------------------------------------

/* this function assumes that p->db_header is valid,             */
/*   that p->db_header.in_transaction == 0, and that             */
/*   p->db_header.rollback_link != 0L                            */
/* Neither the cache nor the transaction mecanism must be used ! */
/* In case of failure, the db_header is invalid afterwards !     */
/* this function can possibly be interrupted and called again.   */

int deallocate_rollback_chain (ref DB_INFO p);

//----------------------------------------------------------------------------
#end unsafe
//----------------------------------------------------------------------------
