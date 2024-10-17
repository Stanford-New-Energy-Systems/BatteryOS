#ifndef REFRESH_HPP
#define REFRESH_HPP

/**
 * Battery Refresh Mode
 * LAZY:   refresh if the status is older than the maximum staleness
 * ACTIVE: automatically refresh in background with period of maximum staleness
 */
enum class RefreshMode : int {
    LAZY   = 0,
    ACTIVE = 1,
};

#endif
