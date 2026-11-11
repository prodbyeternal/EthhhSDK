#include "player_learning.h"
#include <cstdio>
#include <stdexcept>
#include <limits>

static int checks=0;
static void Check(bool ok,const char* label) { ++checks; if (!ok) throw std::runtime_error(label); }
int main() {
    try {
        eth::game::MovementEvidence player, autonomous, stationary, follower;
        const float dt=1.0f/60;
        // Overlay-paused motion must neither advance time nor improve a rank.
        for(int i=0;i<300;i++) player.Sample(100,dt,true,true);
        Check(player.activeTime==0 && player.Score()==0,"paused samples ignored");
        for(int i=0;i<360;i++) {
            bool moving=i>=60 && i<240;
            float step=moving ? 5*dt : 0;
            player.Sample(step,dt,moving,false);
            follower.Sample(step,dt,moving,false);
            autonomous.Sample(30*dt,dt,moving,false);
            stationary.Sample(0,dt,moving,false);
        }
        Check(player.Score()>4.9f && player.Score()<5.1f,"input-correlated player motion");
        Check(autonomous.Score()==0,"fast autonomous object is not preferred");
        Check(stationary.Score()==0,"stationary object has no evidence");
        Check(follower.Score()==player.Score(),"followers are ambiguous: caller must not auto-pin");
        eth::game::MovementEvidence incomplete;
        incomplete.Sample(10,3,true,false);
        Check(incomplete.Score()==0,"idle evidence required");
        eth::game::MovementEvidence idleOnly, invalid;
        idleOnly.Sample(0,3,false,false);
        Check(idleOnly.Score()==0,"input evidence required");
        invalid.Sample(-1,dt,true,false);
        invalid.Sample(1,0,true,false);
        invalid.Sample(1,-dt,true,false);
        invalid.Sample(std::numeric_limits<float>::quiet_NaN(),dt,true,false);
        invalid.Sample(1,std::numeric_limits<float>::infinity(),true,false);
        Check(invalid.activeTime==0 && invalid.idleTime==0 && invalid.Score()==0,"invalid samples ignored");
        printf("PASS player learning: %d assertions (ranking, not identity)\n",checks);
        return 0;
    } catch(const std::exception& ex) { fprintf(stderr,"%s\n",ex.what()); return 1; }
}
