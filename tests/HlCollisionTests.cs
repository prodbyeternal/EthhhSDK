using System;
using EthhhSDK;
using HalfLifeMovement;

internal static unsafe class HlCollisionTests
{
    private static int checks, calls, maxCount;
    private static float floorY, ceilingY, wallX, wallZ, slope;
    private static int invalid;
    private static bool fail, floorEnabled;
    private static void Check(bool ok, string label) { checks++; if (!ok) throw new Exception(label); }
    private static void Near(float a, float b, string label, float epsilon=0.0001f) { Check(!float.IsNaN(a) && Math.Abs(a-b)<=epsilon, label+": "+a+" != "+b); }
    private static void Reset()
    {
        floorY=0; ceilingY=wallX=wallZ=float.PositiveInfinity; slope=0;
        fail=false; floorEnabled=true; invalid=0; calls=maxCount=0;
    }
    private static bool Trace(uint player, TraceSegment[] segments, TraceHit[] hits)
    {
        calls++; maxCount=Math.Max(maxCount,segments.Length);
        if(fail) return false;
        for(int i=0;i<segments.Length;i++) {
            hits[i]=default; var s=segments[i];
            var start=new Vec3(s.StartX,s.StartY,s.StartZ);
            var delta=new Vec3(s.EndX-s.StartX,s.EndY-s.StartY,s.EndZ-s.StartZ);
            float best=2; Vec3 normal=Vec3.Zero;
            if(floorEnabled) Plane(start,delta,new Vec3(-slope,1,0).Normalized(),floorY/(float)Math.Sqrt(1+slope*slope),ref best,ref normal);
            if(!float.IsInfinity(ceilingY)) Plane(start,delta,new Vec3(0,-1,0),-ceilingY,ref best,ref normal);
            if(!float.IsInfinity(wallX)) Plane(start,delta,new Vec3(-1,0,0),-wallX,ref best,ref normal);
            if(!float.IsInfinity(wallZ)) Plane(start,delta,new Vec3(0,0,-1),-wallZ,ref best,ref normal);
            if(best<=1) {
                Vec3 point=start+delta*best;
                var hit=new TraceHit { Hit=1 };
                hit.Point[0]=point.X; hit.Point[1]=point.Y; hit.Point[2]=point.Z;
                hit.Normal[0]=normal.X; hit.Normal[1]=normal.Y; hit.Normal[2]=normal.Z;
                if(invalid==1) hit.Point[0]=float.NaN;
                if(invalid==2) hit.Normal[0]=hit.Normal[1]=hit.Normal[2]=0;
                if(invalid==3) hit.Normal[1]=float.PositiveInfinity;
                if(invalid==4) { hit.Point[0]+=10; hit.Point[2]+=10; }
                hits[i]=hit;
            }
        }
        return true;
    }
    private static void Plane(Vec3 start,Vec3 delta,Vec3 n,float d,ref float best,ref Vec3 normal)
    {
        float dot=Vec3.Dot(delta,n);
        if(Math.Abs(dot)<1e-8f) return;
        float t=(d-Vec3.Dot(start,n))/dot;
        if(t>=0 && t<=1 && t<best) { best=t; normal=n; }
    }
    public static int Main()
    {
        try {
            var collision=new HlCollision(Trace);
            foreach(float scale in new[]{.001f,.025f,.05f,.1f,1f}) {
                Reset(); Vec3 normal; float ground; bool supported;
                Check(collision.TryGround(0x1000,Vec3.Zero,scale,out supported,out ground,out normal)&&supported,"flat support"); Near(ground,0,"floor height");
                Check(collision.TryGround(0x1000,new Vec3(0,3*scale,0),scale,out supported,out ground,out normal)&&!supported,"short-fall gap not grounded");
                slope=.8f;
                Check(collision.TryGround(0x1000,new Vec3(0,12*.8f*scale,0),scale,out supported,out ground,out normal)&&supported,"walkable slope");
                slope=1.5f;
                Check(collision.TryGround(0x1000,Vec3.Zero,scale,out supported,out ground,out normal)&&!supported,"steep wall is not support");
                Near(normal.Y,0.555f,"steep surface normal still reported",0.01f);
                bool contact; Vec3 wallNormal; float wallFacing; Vec3 slidePos;
                Reset(); wallX=12*scale;
                Check(collision.TryWallProbe(0x1000,Vec3.Zero,new Vec3(1,0,0),scale,out contact,out wallNormal,out wallFacing),"wall probe query");
                Check(contact && wallNormal.X < -0.99f,"facing wall detected");
                Near(wallFacing,1,"facing dot");
                Check(calls==1 && maxCount==16,"wall probe is one 16-trace batch");
                Check(collision.TryWallProbe(0x1000,Vec3.Zero,new Vec3(-1,0,0),scale,out contact,out wallNormal,out wallFacing)&&!contact,"wall behind the view ignored");
                Reset();
                Check(collision.TryWallProbe(0x1000,Vec3.Zero,new Vec3(1,0,0),scale,out contact,out wallNormal,out wallFacing)&&!contact,"no wall is a successful empty probe");
                Reset(); slope=1.5f;
                Check(collision.TryWallProbe(0x1000,new Vec3(0,-20*scale,0),new Vec3(1,0,0),scale,out contact,out wallNormal,out wallFacing)&&contact,"steep half-pipe is wall-probed");
                Near(wallNormal.Y,0.555f,"half-pipe normal",0.01f);
                Reset(); slope=0.8f;
                Check(collision.TryWallProbe(0x1000,new Vec3(0,-20*scale,0),new Vec3(1,0,0),scale,out contact,out wallNormal,out wallFacing)&&!contact,"walkable slope is not a wall");
                Reset(); floorY=0;
                Check(collision.TryStepSupport(0x1000,Vec3.Zero,scale,out supported,out ground,out normal)&&supported,"flat step support");
                Near(ground,0,"flat step height");
                Reset(); floorY=5*scale;
                Check(collision.TryStepSupport(0x1000,Vec3.Zero,scale,out supported,out ground,out normal)&&supported,"rise within step");
                Near(ground,5*scale,"step-up height");
                Reset(); floorY=25*scale;
                Check(collision.TryStepSupport(0x1000,Vec3.Zero,scale,out supported,out ground,out normal)&&!supported,"high ledge outside step");
                Reset(); floorY=-10*scale;
                Check(collision.TryStepSupport(0x1000,Vec3.Zero,scale,out supported,out ground,out normal)&&supported,"drop within step");
                Near(ground,-10*scale,"step-down height");
                Reset(); slope=1.5f;
                Check(collision.TryStepSupport(0x1000,Vec3.Zero,scale,out supported,out ground,out normal)&&supported,"steep half-pipe step support");
                Near(normal.Y,0.555f,"step support reports steep normal",0.01f);
                Reset(); slope=1.5f; var steepVelocity=new Vec3(320,0,0);
                Check(collision.TryHorizontal(0x1000,new Vec3(0,10*scale,0),new Vec3(20*scale,0,0),scale,ref steepVelocity,out slidePos,out bool steepWall,out normal),"steep surface horizontal sweep");
                Check(!steepWall && Math.Abs(slidePos.X-20*scale)<0.001f,"steep surface does not block normal movement");
                steepVelocity=new Vec3(320,0,0);
                Check(collision.TryHorizontal(0x1000,new Vec3(0,10*scale,0),new Vec3(20*scale,0,0),scale,ref steepVelocity,out slidePos,out steepWall,out normal,HlCollision.WalkableNormalY)&&steepWall,"wall mode blocks the same steep surface");
                Reset(); floorEnabled=false;
                Check(collision.TryGround(0x1000,Vec3.Zero,scale,out supported,out ground,out normal)&&!supported,"missing floor is successful no-support");
                Reset(); wallX=40*scale; wallZ=40*scale;
                var velocity=new Vec3(320,0,320);
                Check(collision.TryHorizontal(0x1000,Vec3.Zero,new Vec3(60*scale,0,60*scale),scale,ref velocity,out Vec3 position,out bool wall,out normal)&&wall,"corner sweep");
                Check(position.X<=wallX-12*scale+0.001f && position.Z<=wallZ-12*scale+0.001f,"radius kept out of both walls");
                Check(Math.Abs(velocity.X)<.001f && Math.Abs(velocity.Z)<.001f,"corner clips velocity");
                Near(normal.Length,1,"wall normal is returned normalized");
                Near(normal.Y,0,"wall normal keeps its surface component");
                Check(calls<=3 && maxCount==9,"bounded body traces");
                Reset(); wallX=40*scale; velocity=new Vec3(320,0,320);
                Check(collision.TryHorizontal(0x1000,Vec3.Zero,new Vec3(60*scale,0,60*scale),scale,ref velocity,out position,out wall,out normal)&&wall,"wall sweep");
                Check(position.Z>40*scale && position.X<=wallX-12*scale+0.001f,"unconsumed tangent re-swept along wall");
                velocity=new Vec3(-320,0,0);
                Check(collision.TryHorizontal(0x1000,position,new Vec3(-5*scale,0,0),scale,ref velocity,out Vec3 away,out wall,out normal)&&away.X<position.X,"can retreat from contact");
                Reset(); ceilingY=80*scale;
                Check(collision.TryVertical(0x1000,Vec3.Zero,30*scale,scale,out float y,out bool floor,out bool ceiling)&&ceiling&&!floor,"head ceiling sweep");
                Near(y,7.5f*scale,"ceiling radius/skin");
                floorY=-20*scale;
                Check(collision.TryVertical(0x1000,new Vec3(0,10*scale,0),-40*scale,scale,out y,out floor,out ceiling)&&floor&&!ceiling,"downward landing below takeoff");
                Near(y,floorY,"landing at actual floor");
                floorY=0;
                Check(collision.TryVertical(0x1000,new Vec3(0,.25f*scale,0),-.25f*scale,scale,out y,out floor,out ceiling)&&floor,"sub-skin landing cannot penetrate floor");
                Near(y,0,"sub-skin landing height");
                floorEnabled=false;
                Check(collision.TryVertical(0x1000,Vec3.Zero,-40*scale,scale,out y,out floor,out ceiling)&&!floor&&!ceiling,"no imaginary takeoff floor"); Near(y,-40*scale,"unobstructed descent");
                Reset(); fail=true; velocity=Vec3.Zero;
                Check(!collision.TryGround(0x1000,Vec3.Zero,scale,out supported,out ground,out normal),"failed support query refused");
                Check(!collision.TryHorizontal(0x1000,Vec3.Zero,new Vec3(scale,0,0),scale,ref velocity,out position,out wall,out normal),"failed body query refused");
                Check(!collision.TryVertical(0x1000,Vec3.Zero,-scale,scale,out y,out floor,out ceiling),"failed vertical query refused");
                foreach(int mode in new[]{1,2,3,4}) {
                    Reset(); invalid=mode;
                    Check(!collision.TryGround(0x1000,Vec3.Zero,scale,out supported,out ground,out normal),"invalid support hit "+mode);
                    Check(!collision.TryVertical(0x1000,new Vec3(0,scale,0),-scale,scale,out y,out floor,out ceiling),"invalid vertical hit "+mode);
                    Check(!collision.TryStepSupport(0x1000,Vec3.Zero,scale,out supported,out ground,out normal),"invalid step hit "+mode);
                    wallX=12*scale;
                    Check(!collision.TryWallProbe(0x1000,Vec3.Zero,new Vec3(1,0,0),scale,out contact,out wallNormal,out wallFacing),"invalid wall probe hit "+mode);
                    wallX=20*scale; velocity=new Vec3(320,0,0);
                    Check(!collision.TryHorizontal(0x1000,Vec3.Zero,new Vec3(30*scale,0,0),scale,ref velocity,out position,out wall,out normal),"invalid wall hit "+mode);
                }
                Reset(); var rng=new Random(173);
                for(int i=0;i<300;i++) {
                    wallX=wallZ=40*scale;
                    Vec3 delta=new Vec3((float)rng.NextDouble()*90*scale,0,(float)rng.NextDouble()*90*scale);
                    velocity=delta/scale; calls=0;
                    Check(collision.TryHorizontal(0x1000,Vec3.Zero,delta,scale,ref velocity,out position,out wall,out normal),"random corner query");
                    Check(position.X<=wallX-12*scale+.001f && position.Z<=wallZ-12*scale+.001f,"random radius containment");
                    Check(calls<=3 && maxCount<=9,"random query budget");
                }
            }
            Vec3 desired=new Vec3(3,0,4);
            var remaining=HlCollision.RemainingHorizontal(desired,new Vec3(3,5,4),true); Near(remaining.Length,0,"matching native walk credited");
            remaining=HlCollision.RemainingHorizontal(desired,new Vec3(-3,5,-4),true); Near(remaining.Length,5,"opposing native correction not undone");
            remaining=HlCollision.RemainingHorizontal(desired,new Vec3(30,5,40),true); Near(remaining.Length,0,"large native advance not rewound");
            remaining=HlCollision.RemainingHorizontal(desired,new Vec3(3,5,4),false); Near(remaining.Length,5,"additive mode");
            float v=268.328f, offset=0;
            for(int i=0;i<90;i++) offset+=HlJump.Integrate(ref v,800,1.0f/60);
            Check(offset<0 && v<0,"HL integrator continues below takeoff until actual support");
            Console.WriteLine("PASS HL segment collision: "+checks+" assertions (synthetic geometry, NOT native hull/OOB acceptance)");
            return 0;
        } catch(Exception ex) { Console.Error.WriteLine(ex); return 1; }
    }
}
