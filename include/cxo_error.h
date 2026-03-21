/*
 * cxo_error.h - CXO Error Codes
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#ifndef CXO_ERROR_H
#define CXO_ERROR_H

/* Success */
#define CXO_OK          0

/* General errors */
#define CXO_ERR_NOMEM   -1   /* Out of memory */
#define CXO_ERR_IO      -2   /* I/O error */
#define CXO_ERR_INVAL   -3   /* Invalid argument */

/* Scanner errors */
#define CXO_ERR_SCAN    -10  /* Scan failed */
#define CXO_ERR_NODIR   -11  /* Directory not found */
#define CXO_ERR_TOOMANY -12  /* Too many entries */

/* Parser errors */
#define CXO_ERR_PARSE   -20  /* Parse failed */
#define CXO_ERR_NOFILE  -21  /* File not found */
#define CXO_ERR_FMT     -22  /* Invalid format */

/* Linker errors */
#define CXO_ERR_LINK    -30  /* Link failed */
#define CXO_ERR_DUPID   -31  /* Duplicate entry id */

/* Renderer errors */
#define CXO_ERR_RENDER  -40  /* Render failed */
#define CXO_ERR_NOTHEME -41  /* Theme not found */

/* Helper to check if return value is error */
#define CXO_IS_ERR(rc)  ((rc) < 0)

#endif /* CXO_ERROR_H */
