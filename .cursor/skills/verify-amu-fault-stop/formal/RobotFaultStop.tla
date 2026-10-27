---------------------------- MODULE RobotFaultStop ----------------------------
(***************************************************************************)
(* Models the AMU STOP/FAULT path for B1 and wheeldrop.                     *)
(*                                                                          *)
(* Code covered (see skill sync commit):                                    *)
(*   Src/FSM_state/states/action_fault.c  — fault conditions               *)
(*   Src/FSM_state/states/state_fault.c   — Fault init drives STOP         *)
(*   Src/FSM/fsm.c                        — one Action per Tick            *)
(*   Src/FSM_state/states/state_idle.c    — Idle has no B1→FAULT Action    *)
(*                                                                          *)
(* Abstraction: every non-Idle non-Fault State that wires                   *)
(* action_fault_b1_wheeldrop or action_fault_all is "Active". One Tick      *)
(* samples sensors then runs the first true Action. Active checks the       *)
(* B1/wheeldrop fault Action before other work. Fault returns to Idle.      *)
(*                                                                          *)
(* Property: if B1 is held or a wheel is dropped (in the sampled sensors),  *)
(* motors are not Running. Harm: driving while lifted or ignoring stop.     *)
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

\* Sample fresh sensors, then react (Kobuki_Read then FSM_Tick).
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
                  /\ motors' = Stopped
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
