#ifndef MOOLAHCLI_NAMESPACE_H
#define MOOLAHCLI_NAMESPACE_H

/* Keep the CLI library's independent Value, map, and string implementations
 * from colliding with the interpreter's corresponding symbols. */
#define ValueNew MoolahValueNew
#define ValueNewEx MoolahValueNewEx
#define ValueFree MoolahValueFree
#define ValueDeepFree MoolahValueDeepFree
#define ValueAppend MoolahValueAppend
#define ValueLen MoolahValueLen
#define MapNew MoolahMapNew
#define MapSet MoolahMapSet
#define MapGet MoolahMapGet
#define MapFree MoolahMapFree
#define strf MoolahStrf
#define appendf MoolahAppendf

#endif
