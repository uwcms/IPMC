const long int GIT_SHORT_INT __attribute__((used)) = D_GIT_SHORT_INT; // const ints are static storage by default, we must ensure this one is not eliminated by the compiler.
const char *GIT_SHORT        = D_GIT_SHORT;
const char *GIT_LONG         = D_GIT_LONG;
const char *GIT_DESCRIBE     = D_GIT_DESCRIBE;
const char *GIT_BRANCH       = D_GIT_BRANCH;
const char *GIT_STATUS       = D_GIT_STATUS;
