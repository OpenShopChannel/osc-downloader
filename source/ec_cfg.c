#include <gccore.h>
#include <string.h>

#include "ec_cfg.h"
#include "utils.h"

// A shared ECCfg, utilized to query values whenever needed.
static struct ECCfg ecCfg;

// Loads an ec.cfg to the shared structure.
// If not possible, returns -1. errorMessage and
// errorCode will be updated by this function.
s32 ecInitCfg() {
  u32 length = 0;
  void* data = ISFS_GetFile(EC_CFG_PATH, &length);
  if (data == NULL) {
    // An error message is already present via ISFS_GetFile.
    return -1;
  }

  ecCfg.length = length;
  ecCfg.data = data;

  return 0;
} 

// Searches the loaded ec.cfg for the given key name.
// If unsuccessful, returns NULL.
// If not, it returns a direct pointer to the
// given key's value. For this reason, do not
// mutate the returned value.
char* ecGetKeyValue(char* searchedName) {
  // If there is no configuration loaded, return null.
  if (ecCfg.data == NULL) {
    return NULL;
  }

  int pos = 0;
  while (pos < ecCfg.length) {
    // We use strlen as it scans up until the first null byte.
    // This is the separator utilized within ec.cfg between data.
    int dataLen = strlen(ecCfg.data + pos);

    // Check if this data is a value.
    // If so, carry on. We're looking for a key.
    // Values begin with an equals sign (=)
    // and continue with their data until a null.
    if (ecCfg.data[pos] == '=') {
      // We add one 1 in order to avoid the null byte.
      pos += dataLen + 1;
      continue;
    }

    // Determine if this is the key we are looking for.
    if (strcmp(ecCfg.data + pos, searchedName) != 0) {
      // This is not.
      pos += dataLen + 1;
      continue;
    }

    // We've found our key. We now need to find our value.
    // We adjust our position to the next data, which must be our pair.
    pos += dataLen + 1;

    // Ensure this is a value by checking its leading =.
    if (ecCfg.data[pos] != '=') {
      // This file is most likely corrupt.
      return NULL;
    }

    // We treat our buffer as immutable, so we return an
    // offset of our buffer + 1 in order to read past the leading =.
    // (It is not, given C, but we treat it as such.)
    return ecCfg.data + pos + 1;
  }

  return NULL;
}