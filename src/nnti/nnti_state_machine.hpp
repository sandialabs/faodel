// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#ifndef NNTI_STATE_MACHINE_HPP_
#define NNTI_STATE_MACHINE_HPP_

namespace nnti {
namespace core {

class state_machine {
public:
    virtual int update(NNTI_event_t *event) = 0;
};

}
}

#endif /* NNTI_STATE_MACHINE_HPP_ */
