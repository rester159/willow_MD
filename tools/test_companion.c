#include <assert.h>
#include <stdio.h>
#include "../src/companion_control.h"
int main(void)
{
    short floor[1024];Companion p={0};int i,x;
    for(i=0;i<1024;i++) floor[i]=192;
    companionStep(&p,CP_RIGHT,0,160,floor,1024);
    assert(!p.joined && p.x==0);
    companionSpawn(&p,160,floor,1024);
    assert(p.joined && p.x==128 && p.feet==192);
    companionStep(&p,CP_RIGHT,0,160,floor,1024);
    assert(p.x==130 && !p.facing);
    companionStep(&p,0,CP_JUMP,160,floor,1024);
    assert(!p.grounded && p.feet<192 && p.pose==5);
    for(i=0;i<100;i++)companionStep(&p,0,0,160,floor,1024);
    assert(p.grounded && p.feet==192);
    companionStep(&p,0,CP_ATTACK,160,floor,1024);x=p.x;
    for(i=0;i<8;i++)companionStep(&p,CP_RIGHT,0,160,floor,1024);
    assert(p.x==x && p.pose==7);
    for(i=0;i<300;i++)companionStep(&p,CP_RIGHT,0,160,floor,1024);
    assert(p.x==400);
    p.feet=400;p.yFixed=400*256;
    companionStep(&p,0,0,160,floor,1024);
    assert(p.feet==192 && p.x==128);
    for(i=120;i<150;i++)floor[i]=-1;
    companionSpawn(&p,160,floor,1024);
    assert(floor[p.x+8]>=0);
    for(i=120;i<150;i++)floor[i]=254;
    companionSpawn(&p,160,floor,1024);
    assert(p.feet<=216);
    puts("Companion: join, movement, jump/landing, punch, camera tether and pit recovery passed.");
}
