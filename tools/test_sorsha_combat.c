#include <assert.h>
#include <stdio.h>
#include "../src/sorsha_combat.h"

int main(void)
{
    SorshaCombat s;
    int i, health;
    sorshaReset(&s);
    sorshaStep(&s,S_LEFT,0);
    assert(s.x==86 && s.facing==-1);
    sorshaStep(&s,S_DOWN|S_RIGHT,0);
    assert(s.x==86 && s.frame==5);
    sorshaStep(&s,0,S_JUMP);
    assert(s.height>0 && s.frame==6);
    for(i=0;i<80;i++) sorshaStep(&s,0,0);
    assert(s.height==0 && s.velocity==0);

    /* One sword swing cannot apply damage every tick of its active window. */
    sorshaReset(&s);s.enemyX=s.x+30;
    sorshaStep(&s,0,S_ATTACK);
    for(i=0;i<24;i++) sorshaStep(&s,0,0);
    assert(s.enemyHealth==2);
    sorshaReset(&s);s.enemyX=s.x-30;
    sorshaStep(&s,0,S_ATTACK);
    for(i=0;i<24;i++) sorshaStep(&s,0,0);
    assert(s.enemyHealth==3);

    sorshaReset(&s);s.enemyX=s.x+30;s.enemyTimer=47;
    sorshaStep(&s,S_GUARD,0);
    assert(s.health==5 && s.guarding);
    sorshaReset(&s);s.enemyX=s.x-30;s.enemyTimer=47;
    sorshaStep(&s,S_GUARD,0);
    assert(s.health==4 && s.hurt && s.invulnerable);
    health=s.health;s.enemyTimer=47;
    sorshaStep(&s,0,0);
    assert(s.health==health);

    sorshaReset(&s);s.enemyX=s.x+30;
    sorshaStep(&s,S_DOWN,S_ATTACK);
    for(i=0;i<7;i++) sorshaStep(&s,S_DOWN,0);
    assert(s.frame==12 && s.enemyHealth==2);
    sorshaReset(&s);sorshaStep(&s,0,S_JUMP|S_ATTACK);
    for(i=0;i<7;i++) sorshaStep(&s,0,0);
    assert(s.frame==13);

    sorshaReset(&s);s.defeated=2;s.enemyHealth=1;s.enemyX=s.x+30;
    sorshaStep(&s,0,S_ATTACK);
    for(i=0;i<8;i++) sorshaStep(&s,0,0);
    assert(s.won && s.defeated==3 && s.frame==15);
    health=s.tick;sorshaStep(&s,S_LEFT,S_JUMP);
    assert(s.tick==health);
    puts("Sorsha combat: movement, jump, hit windows, facing, guard, invulnerability, attack poses and victory passed.");
    return 0;
}
