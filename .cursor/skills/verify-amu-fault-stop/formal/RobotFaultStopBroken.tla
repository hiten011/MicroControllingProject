------------------------- MODULE RobotFaultStopBroken -------------------------
(***************************************************************************)
(* Broken variant: Active enters Fault on B1/wheeldrop but leaves motors    *)
(* Running (as if Fault init omitted Kobuki_Drive(STOP)).                   *)
(***************************************************************************)

EXTENDS Naturals

CONSTANTS Idle, Active, Fault, Stopped, Running

VARIABLES state, motors, b1, wheeldrop

vars == <<state, motors, b1, wheeldrop>>

TypeOK ==
  /\ state \in {Idle, Active, Fault}
  /\ motors \in {Stopped, Running}
  /\ b1 \in {0, 1}
  /\ wheeldrop \in {0, 1}

StopRequested == (b1 = 1) \/ (wheeldrop = 1)

Sample(stop, nb, nw) ==
  /\ b1' = nb
  /\ wheeldrop' = nw
  /\ stop = ((nb = 1) \/ (nw = 1))

TickIdle ==
  /\ state = Idle
  /\ \E nb, nw \in {0, 1}:
       LET stop == (nb = 1) \/ (nw = 1) IN
       /\ Sample(stop, nb, nw)
       /\ motors' = Stopped
       /\ IF stop THEN state' = Idle ELSE state' \in {Idle, Active}

TickActive ==
  /\ state = Active
  /\ \E nb, nw \in {0, 1}:
       LET stop == (nb = 1) \/ (nw = 1) IN
       /\ Sample(stop, nb, nw)
       /\ IF stop
             THEN /\ state' = Fault
                  /\ motors' = Running
             ELSE /\ state' \in {Active, Fault}
                  /\ motors' \in {Stopped, Running}

TickFault ==
  /\ state = Fault
  /\ \E nb, nw \in {0, 1}:
       LET stop == (nb = 1) \/ (nw = 1) IN
       /\ Sample(stop, nb, nw)
       /\ state' = Idle
       /\ motors' = Stopped

Next == TickIdle \/ TickActive \/ TickFault

Init ==
  /\ state = Idle
  /\ motors = Stopped
  /\ b1 = 0
  /\ wheeldrop = 0

Spec == Init /\ [][Next]_vars

SafeMotors == StopRequested => motors = Stopped

=============================================================================
