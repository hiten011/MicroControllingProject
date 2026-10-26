#ifndef ACTION_FAULT_H
#define ACTION_FAULT_H

#include "fsm.h"

// B1, wheeldrop, any bumper or any cliff: go to FAULT. Listed first in SEARCH, DETECT and SPIN.
extern const FSM_Action action_fault_all;

// B1, wheeldrop or any bumper: go to FAULT. The cliff is ignored. Listed first in ALIGN.
extern const FSM_Action action_fault_ignore_cliff;

#endif
