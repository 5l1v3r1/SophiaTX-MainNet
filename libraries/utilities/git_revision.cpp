#include <stdint.h>
#include <steem/utilities/git_revision.hpp>

#define STEEM_GIT_REVISION_SHA "e1edc049b6e3d78a2e6bdec638d16a25827190ae"
#define STEEM_GIT_REVISION_UNIX_TIMESTAMP 1525253904
#define STEEM_GIT_REVISION_DESCRIPTION "unknown"

namespace steem { namespace utilities {

const char* const git_revision_sha = STEEM_GIT_REVISION_SHA;
const uint32_t git_revision_unix_timestamp = STEEM_GIT_REVISION_UNIX_TIMESTAMP;
const char* const git_revision_description = STEEM_GIT_REVISION_DESCRIPTION;

} } // end namespace steem::utilities
