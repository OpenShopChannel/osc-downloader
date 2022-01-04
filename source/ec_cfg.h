// Within the Open Shop Channel, we have renamed ec.cfg to osc.cfg
// in order to not conflict with existing user data via Nintendo.
#define EC_CFG_PATH "/title/00010002/48414241/data/osc.cfg"

// ECCfg is a wrapping struct around a given ec.cfg's contents.
// As there are many null bytes within ec.cfg,
// it's important that the length of our data is preserved.
struct ECCfg {
  int length;
  char* data;
};

// Loads an ec.cfg to the shared structure.
// If not possible, returns -1. errorMessage and
// errorCode will be updated by this function.
s32 ecInitCfg();

// Searches the loaded ec.cfg for the given key name.
// If unsuccessful, returns NULL.
// If not, it returns a direct pointer to the
// given key's value. For this reason, do not
// mutate the returned value.
char* ecGetKeyValue(char* searchedName);