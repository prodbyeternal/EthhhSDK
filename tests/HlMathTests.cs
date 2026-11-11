using System;
using HalfLifeMovement;

internal static class HlMathTests
{
    private static int checks;
    private static void Near(float actual, float expected, string label, float epsilon = 0.001f)
    {
        checks++;
        if (float.IsNaN(actual) || float.IsInfinity(actual) || Math.Abs(actual - expected) > epsilon)
            throw new Exception(label + ": expected " + expected + ", got " + actual);
    }
    private static void Vector(Vec3 actual, Vec3 expected, string label)
    { Near(actual.X, expected.X, label+" X"); Near(actual.Y, expected.Y, label+" Y"); Near(actual.Z, expected.Z, label+" Z"); }

    public static int Main()
    {
        try
        {
            var t = new HlTuning(); var move = new HlMove(); const float dt = 1.0f/60;
            var x = new Vec3(1,0,0);
            move.Accelerate(x,320,10,dt); Near(move.Speed,53.333333f,"first ground acceleration");
            for(int i=0;i<10;i++) move.Accelerate(x,320,10,dt);
            Near(move.Speed,320,"ground speed cap");
            move.Velocity=new Vec3(600,0,0); move.Accelerate(x,320,10,dt);
            Near(move.Speed,600,"no acceleration above directional wishspeed");
            move.ApplyFriction(dt,t); Near(move.Speed,560,"high-speed friction");
            move.Velocity=new Vec3(50,0,0); move.ApplyFriction(dt,t); Near(move.Speed,43.333333f,"stopspeed friction");
            move.ApplyFriction(1,t); Near(move.Speed,0,"friction cannot reverse velocity");
            move.Reset(); move.AirAccelerate(x,320,10,0.001f,t);
            Near(move.Speed,3.2f,"air accel uses uncapped wishspeed multiplier");
            move.AirAccelerate(x,320,10,dt,t); Near(move.Speed,30,"air directional cap");
            move.Velocity=new Vec3(320,0,0); move.AirAccelerate(new Vec3(0,0,1),320,10,dt,t);
            Vector(move.Velocity,new Vec3(320,0,30),"air strafe preserves orthogonal speed");
            move.Velocity=new Vec3(544,0,0); move.CapBunnySpeed(t); Near(move.Speed,544,"bunny threshold unchanged");
            move.Velocity=new Vec3(600,0,0); move.CapBunnySpeed(t); Near(move.Speed,353.6f,"bunny crop 1.7 * 320 * .65");
            move.Velocity=new Vec3(float.NaN,float.PositiveInfinity,float.NegativeInfinity); move.ClampVelocity(t);
            Vector(move.Velocity,new Vec3(0,2000,-2000),"finite velocity cleanup");

            // Differential scalar equations from the authorized legacy HLSDK:
            // PM_Friction (normal floor, no edge trace), PM_Accelerate and
            // PM_AirAccelerate; I-Ninja maps HL's Z-up to assumed Y-up.
            var random = new Random(421);
            for(int i=0;i<2000;i++)
            {
                float angle=(float)(random.NextDouble()*Math.PI*2);
                var direction=new Vec3((float)Math.Cos(angle),0,(float)Math.Sin(angle));
                var before=new Vec3((float)(random.NextDouble()*1200-600),0,(float)(random.NextDouble()*1200-600));
                float wish=(float)(random.NextDouble()*320), step=(float)(random.NextDouble()*0.04);
                move.Velocity=before; move.Accelerate(direction,wish,10,step);
                float add=wish-Vec3.Dot(before,direction);
                var expected=add<=0 ? before : before+direction*Math.Min(add,10*step*wish);
                Vector(move.Velocity,expected,"ground reference "+i);
                move.Velocity=before; move.AirAccelerate(direction,wish,10,step,t);
                add=Math.Min(wish,30)-Vec3.Dot(before,direction);
                expected=add<=0 ? before : before+direction*Math.Min(add,10*wish*step);
                Vector(move.Velocity,expected,"air reference "+i);
                move.Velocity=before; move.ApplyFriction(step,t);
                float speed=before.Length;
                expected=speed<.1f ? before : before*(Math.Max(0,speed-Math.Max(speed,100)*4*step)/speed);
                Vector(move.Velocity,expected,"friction reference "+i);
            }

            float jumpSpeed=(float)Math.Sqrt(2*800*45), height=0, vertical=jumpSpeed, maxHeight=0;
            float baseY=1234, writtenY=baseY;
            for(int i=1;i<=60;i++)
            {
                HlJump.Step(ref height,ref vertical,800,dt);
                float time=i*dt;
                Near(height,Math.Max(0,jumpSpeed*time-400*time*time),"analytic anchored arc "+i,.002f);
                writtenY=baseY+height; maxHeight=Math.Max(maxHeight,height);
            }
            Near(writtenY,baseY,"lands at takeoff, never adds carry repeatedly");
            Near(maxHeight,45,"45-unit apex",.01f); Near(vertical,0,"landed vertical velocity");
            height=0; vertical=jumpSpeed;
            for(int i=0;i<20;i++) HlJump.Step(ref height,ref vertical,800,dt);
            float original=height; HlJump.Step(ref height,ref vertical,800,0);
            Near(height,original,"zero-dt pause preserves arc");

            var matrix=new float[16]; var position=new Vec3(1,2,3);
            HlCamera.MakeWorld(position,0,0,0,matrix);
            Vector(new Vec3(matrix[0],matrix[1],matrix[2]),x,"camera yaw zero right");
            Vector(new Vec3(matrix[8],matrix[9],matrix[10]),new Vec3(0,0,1),"camera yaw zero forward");
            HlCamera.MakeWorld(position,(float)Math.PI/2,0,0,matrix);
            Vector(new Vec3(matrix[8],matrix[9],matrix[10]),x,"90-degree yaw forward");
            for(int i=0;i<512;i++)
            {
                float yaw=(float)(random.NextDouble()*Math.PI*2), pitch=(float)((random.NextDouble()*178-89)*Math.PI/180);
                float roll=(float)(random.NextDouble()*Math.PI*2);
                HlCamera.MakeWorld(position,yaw,pitch,roll,matrix);
                var right=new Vec3(matrix[0],matrix[1],matrix[2]);
                var up=new Vec3(matrix[4],matrix[5],matrix[6]);
                var forward=new Vec3(matrix[8],matrix[9],matrix[10]);
                Near(right.Length,1,"unit right"); Near(up.Length,1,"unit up"); Near(forward.Length,1,"unit forward");
                Near(Vec3.Dot(right,up),0,"right/up orthogonal"); Near(Vec3.Dot(right,forward),0,"right/forward orthogonal");
                Near(Vec3.Dot(up,forward),0,"up/forward orthogonal");
                Near(Vec3.Dot(Vec3.Cross(right,up),forward),1,"positive determinant");
                Vector(new Vec3(matrix[12],matrix[13],matrix[14]),position,"row-vector translation");
                Near(matrix[15],1,"homogeneous W");
            }
            Console.WriteLine("PASS HL math: " + checks + " assertions (legacy math; collision/world axes NOT verified)");
            return 0;
        }
        catch(Exception ex) { Console.Error.WriteLine(ex); return 1; }
    }
}
