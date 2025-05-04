#pragma once
// project
#include "clay/utils/common/Utils.h"

namespace clay {

class Audio {
public:
    Audio(utils::FileData& fileData);

    /** @brief Destructor */
    ~Audio();

    /** Get the AL buffer id */
    int getId();

private:
    /** AL audio id */
    unsigned int mId_;
};

} // namespace clay