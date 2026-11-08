#ifndef WILLOW_TEST_SHOP_PURCHASE_ROUTE_H
#define WILLOW_TEST_SHOP_PURCHASE_ROUTE_H
/* Test controller and read-only observations for the existing full scheduler
 * oracle. This file never writes game RAM or supplies task completion. The
 * caller applies returned controller bits identically to all three runs. */
typedef struct {
 unsigned phase,ticks,slot,commands,confirmations,paidPass,textPass,dispatches;
 unsigned initialGold,paidGold,initialLevel,initialMarks,completed;
 unsigned firstMessage,secondMessage,firstStart,firstDone,secondStart;
 unsigned firstDispatches,secondDispatches;
} WillowTestShopPurchaseRoute;
static WillowTestShopPurchaseRoute shopPurchaseRoute;
static unsigned shopPurchaseGold(void)
{
 unsigned value=0;
 for(unsigned n=0;n<4;n++){
  unsigned b=rb(0x8368+n);
  if((b&15)>9||(b>>4)>9){fprintf(stderr,"Invalid source money BCD\n");exit(1);}
  value=value*100+(b>>4)*10+(b&15);
 }
 return value;
}
static uint8_t shopPurchaseController(unsigned entered,uint8_t otherwise,unsigned*inputTicks)
{
 WillowTestShopPurchaseRoute*r=&shopPurchaseRoute;
 if(rw(0x8008)!=4)return otherwise;
 if(!entered)return 0;
 (*inputTicks)++;r->ticks++;
 if(!r->phase){
  r->slot=12;
  for(unsigned n=0;n<12;n++)if(rb(0x0f22+n)==1){r->slot=n;break;}
  r->initialGold=shopPurchaseGold();r->initialLevel=rb(0x8355);r->initialMarks=rw(0x0f14);
  r->firstMessage=romByte(0x33fe6UL+rb(0x0f0e));r->secondMessage=romByte(0x33ff8UL+rb(0x0f0e));
  printf("Paid route initial shop %u gold %u level %u descriptors",rb(0x0f0e),r->initialGold,r->initialLevel);
  for(unsigned n=0;n<12;n++)printf(" %u",rb(0x0f22+n));puts("");fflush(stdout);
  if(r->slot==12||r->initialGold<50||!r->secondMessage||romWord(0x34878)||romWord(0x3487a)!=0x50){fprintf(stderr,"Reached shop cannot exercise original two-message item1 with current earned resources\n");exit(1);}
  r->phase=1;
 }
 if(r->ticks>2048){fprintf(stderr,"Paid route stalled phase%u menu%u/%u task%u slot%u\n",r->phase,rw(0x800c),rw(0x8010),rb(0xc0),r->slot);exit(1);}
 if(r->phase==1&&rw(0x800c)==2&&rw(0x8010)==2)r->phase=2;
 if(r->phase==3)return 0; /* Real text task must finish before any navigation. */
 if(r->phase==4){
  r->slot=12;for(unsigned n=0;n<12;n++)if(rb(0x0f22+n)==6){r->slot=n;break;}
  if(r->slot==12){fprintf(stderr,"Original leave item absent\n");exit(1);}r->phase=5;
 }
 if((r->ticks&3)!=1)return 0; /* Release between each original edge input. */
 uint8_t command=0;
 if(r->phase==1&&!rw(0x8010))command=16;
 else if((r->phase==2||r->phase==5)&&rw(0x800c)==2&&rw(0x8010)==2){
  if(rw(0x0f10)!=(r->slot%4)*2)command=1;
  else if(rw(0x0f12)!=(r->slot/4)*2)command=4;
  else {command=16;r->confirmations++;}
 }
 if(command)r->commands++;
 return command;
}
static void shopPurchaseAfterMain(unsigned pass)
{
 WillowTestShopPurchaseRoute*r=&shopPurchaseRoute;
 if(r->phase==2&&shopPurchaseGold()!=r->initialGold){
  r->paidGold=shopPurchaseGold();
  if(r->initialGold-r->paidGold!=50||rb(0x8355)!=r->initialLevel||
     rw(0x0f14)!=(r->initialMarks|(1u<<r->slot))||rw(0x8010)!=6||rb(0x0f36)){
   fprintf(stderr,"Paid route did not produce original item1 debit, mark and dialogue state\n");exit(1);
  }
  r->paidPass=pass;r->phase=3;
  printf("Paid route debit pass %u gold %u->%u slot %u state6\n",pass,r->initialGold,r->paidGold,r->slot);fflush(stdout);
 }
 if(r->phase!=3)return;
 if(!r->firstStart&&rw(0x8010)==6&&rb(0x0f36)==2){
  if(rb(0xc0)!=12||rw(0xc4)||rw(0xc6)!=0x78f0||rw(0xd0)!=r->firstMessage||rw(0xd2)!=0x1300||!rb(0x0e58)||rb(0x0f37)!=31){
   fprintf(stderr,"Original first paid task/wait setup missing\n");exit(1);
  }
  r->firstStart=pass;printf("Paid first message %u starts pass %u\n",r->firstMessage,pass);fflush(stdout);
 }
 if(r->firstDone&&!r->secondStart&&rw(0x8010)==2){
  if(rb(0xc0)!=12||rw(0xc4)||rw(0xc6)!=0x78f0||rw(0xd0)!=r->secondMessage||rw(0xd2)!=0x1300||rb(0x0f36)||pass-r->firstDone!=31){
   fprintf(stderr,"Original second paid task/31-tick response setup missing pass%u firstDone%u\n",pass,r->firstDone);exit(1);
  }
  r->secondStart=pass;printf("Paid second message %u starts pass %u after31 response ticks\n",r->secondMessage,pass);fflush(stdout);
 }
}
static void shopPurchaseSelected(const WillowTaskSelection*s)
{
 WillowTestShopPurchaseRoute*r=&shopPurchaseRoute;
 if(r->phase==3&&s->dispatch&&s->slot==0xc0){
  r->dispatches++;
  if(r->secondStart)r->secondDispatches++;else if(r->firstStart)r->firstDispatches++;
 }
}
static void shopPurchaseAfterScheduler(unsigned pass,const WillowNativeTaskRuntime*r)
{
 WillowTestShopPurchaseRoute*p=&shopPurchaseRoute;
 if(p->phase!=3||rb(0xc0)||r->resume[6])return;
 if(p->firstStart&&!p->firstDone){
  if(!p->firstDispatches||pass<=p->firstStart||rb(0x0e58)||rb(0x0f37)!=31){fprintf(stderr,"First paid text had no real scheduler lifetime\n");exit(1);}
  p->firstDone=pass;printf("Paid first text complete pass %u / %u real C0 dispatches\n",pass,p->firstDispatches);fflush(stdout);
 }
 if(!p->secondStart)return;
 if(!p->secondDispatches||pass<=p->secondStart){fprintf(stderr,"Second paid text had no real scheduler lifetime\n");exit(1);}
 p->textPass=pass;p->phase=4;p->completed++;
 printf("Paid route text complete pass %u after %u ticks / %u real C0 dispatches\n",pass,pass-p->paidPass,p->dispatches);fflush(stdout);
}
static void shopPurchaseCheck(void)
{
 const WillowTestShopPurchaseRoute*r=&shopPurchaseRoute;
 if(r->phase!=5||r->completed!=1||shopPurchaseGold()!=r->paidGold||rb(0x8355)!=r->initialLevel){
  fprintf(stderr,"Continuous paid purchase did not finish with retained item/gold\n");exit(1);
 }
 printf("Continuous paid purchase: item1,50 earned gold debited,%u real paid-text dispatches,%u dialogue ticks,31 response ticks,controller-only navigation and exit; no completion injection\n",r->dispatches,r->textPass-r->paidPass);
}
#endif
