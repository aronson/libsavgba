#ifndef ERR_DEF_H
#define ERR_DEF_H

enum {
    E_SUCCESS = 0, ///< No error.
    E_INVALID_PARAM, ///< Invalid input parameter.
    E_OUT_OF_RANGE, ///< Address is out of range.
    E_VERIFY_FAIL, ///< Failed to verify written data.
};

#endif /* ERR_DEF_H */
