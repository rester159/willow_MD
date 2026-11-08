/* Audit-only proposal. One fresh instance belongs to one far-row resolution.
 * No global state and no proof survives the current busy preparation call. */
typedef struct {uint16_t row[2],full[2];} BackgroundNearRowProof;
static void rowProofInit(BackgroundNearRowProof*p)
{p->row[0]=p->row[1]=BG_INVALID;p->full[0]=p->full[1]=0;}
static uint16_t rowProofFull(BackgroundNearRowProof*p,uint16_t y)
{
 if(!p)return bgNearVisibleRowFull(y);
 uint16_t slot=y&1;
 if(p->row[slot]!=y){p->full[slot]=bgNearVisibleRowFull(y);p->row[slot]=y;}
 return p->full[slot];
}
