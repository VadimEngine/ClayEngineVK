#pragma once
// project
#include "clay/utils/common/Utils.h"

namespace clay {

class Audio {
public:
    Audio(const utils::FileData& fileData);

    // move constructor
    Audio(Audio&& other);

    // move assignment
    Audio& operator=(Audio&& other) noexcept;

    /** @brief Destructor */
    ~Audio();

    /** Get the AL buffer id */
    int getId();

private:
    /** AL audio id */
    unsigned int mId_;
};

} // namespace clay