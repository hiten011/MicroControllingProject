#ifndef ACTION_FAULT_H
#define ACTION_FAULT_H

#include "fsm.h"

// B1, wheeldrop, any bumper or any cliff: go to FAULT. Listed first in SEARCH, DETECT, SPIN and ORIENT.
extern const FSM_Action action_fault_all;

// B1, wheeldrop or any bumper: go to FAULT. The cliff is ignored. Listed first in ALIGN.
extern const FSM_Action action_fault_ignore_cliff;

// B1 or wheeldrop: go to FAULT. Bumper and cliff ignored. Listed first in PATHFIND.
extern const FSM_Action action_fault_b1_wheeldrop;

#endif
