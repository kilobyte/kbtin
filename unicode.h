#define WC wchar_t
#define WCL sizeof(WC)
#define WCI wint_t
#define WCC "%lc"
#define WClen wcslen
#define WCcpy wcscpy
#define TO_WC(d,s) utf8_to_wc(d,s,-1)
#define FROM_WC(d,s,n) wc_to_utf8(d,s,n,BUFFER_SIZE*8)
#define WRAP_WC(d,s) wc_to_utf8(d,s,-1,BUFFER_SIZE)
#define OUT_WC(d,s,n) wc_to_mb(d,s,n,OUTSTATE)
#define FLATlen utf8_width
int wcwidth(WC ucs);
int wcswidth(const WC *pwcs, size_t n);
