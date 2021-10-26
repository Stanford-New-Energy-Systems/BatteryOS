#ifndef BOSNODE_H
#define BOSNODE_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
/**
 * A node representing a general node in BOS
 */
struct BOSNode {
    uint8_t _dummy;
};
typedef struct BOSNode BOSNode;
#ifdef __cplusplus
}
#endif

#endif // ! BOSNODE_H

