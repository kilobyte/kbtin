#include "tintin.h"
#include "protos/bind.h"
#include "protos/colors.h"
#include "protos/globals.h"
#include "protos/misc.h"
#include "protos/telnet.h"
#include "protos/unicode.h"
#include "protos/user.h"
#include "protos/utils.h"
#ifdef HAVE_TERMIOS_H
# include <termios.h>
#endif
#include <sys/ioctl.h>
#ifdef HAVE_VALGRIND_VALGRIND_H
#include <valgrind/valgrind.h>
#endif

static mbstate_t outstate;
#define OUTSTATE &outstate

static char out_line[BUFFER_SIZE], b_draft[BUFFER_SIZE];
static WC k_input[BUFFER_SIZE], kh_input[BUFFER_SIZE], tk_input[BUFFER_SIZE];
static WC yank_buffer[BUFFER_SIZE];
static char input_color[64];
static int k_len, k_pos, k_scrl, tk_len, tk_pos, tk_scrl;
static int o_len, o_pos, o_oldcolor, o_prevcolor, o_draftlen, o_lastprevcolor;
#define o_color color
#define o_lastcolor lastcolor
static int b_first, b_current, b_last, b_bottom, b_screenb;
static int b_pending_newline;
static bool o_strongdraft;
static int b_greeting;
static char *b_output[CONSOLE_LENGTH];
static int scr_len, scr_curs;
static bool in_getpassword;
static struct termios old_tattr;
static bool retaining;
#ifdef XTERM_TITLE
static bool xterm;
#endif
static bool putty;
static int term_width;
static int dump_color;
static char term_buf[BUFFER_SIZE*8], *tbuf;

static void term_commit(void)
{
    write_stdout(term_buf, tbuf-term_buf);
    tbuf=term_buf;
}

static bool term_init(void)
{
    struct termios tattr;

    if (!(tty=isatty(0)))
    {
        fprintf(stderr, "Warning! isatty() reports stdin is not a terminal.\n");
        return false;
    }

    tcgetattr(0, &old_tattr);
    tattr=old_tattr;
    /* cfmakeraw(&tattr); */
    tattr.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP
                       |INLCR|IGNCR|ICRNL|IXON);
    tattr.c_oflag &= ~OPOST;
    tattr.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
    tattr.c_cflag &= ~(CSIZE|PARENB);
    tattr.c_cflag |= CS8;

#ifndef IGNORE_INT
    tattr.c_lflag|=ISIG;        /* allow C-c, C-\ and C-z */
#endif
    tattr.c_cc[VMIN]=1;
    tattr.c_cc[VTIME]=0;
    tcsetattr(0, TCSANOW, &tattr);

    return true;
}

static void term_restore(void)
{
    tcdrain(0);
    tcsetattr(0, TCSADRAIN, &old_tattr);
}

static void term_getsize(void)
{
    struct winsize ts;

    if (ioctl(1, TIOCGWINSZ, &ts) || ts.ws_row<=0 || ts.ws_col<=0)
    {       /* not a terminal or a broken one, let's quietly assume 80x25 */
        LINES=25;
        COLS=80;
        return;
    }
    LINES=ts.ws_row;
    COLS=ts.ws_col;
}

/* len=-1 for infinite */
static void add_doublewidth(WC *right, WC *left, int len)
{
    while (*left && len--)
        if (isw2width(*left))
        {
            *right++=*left++;
            *right++=EMPTY_CHAR;
        }
        else
            *right++=*left++;
    *right=0;
}

/* len=-1 for infinite */
static void zap_doublewidth(WC *right, const WC *left, int len)
{
    bool norm=false;

    while (len-- && *left)
        if (*left==EMPTY_CHAR)
        {
            if (norm)
                norm=false;
            else
                *right++=' ';
            left++;
        }
        else
        {
            *right++=*left++;
            norm=true;
        }
    *right=0;
}

static void to_wc(WC *d, const char *s)
{
    WC buf[BUFFER_SIZE];

    utf8_to_wc(buf, s, BUFFER_SIZE-1);
    add_doublewidth(d, buf, BUFFER_SIZE);
}

static void wrap_wc(char *d, const WC *s)
{
    WC buf[BUFFER_SIZE];

    zap_doublewidth(buf, s, BUFFER_SIZE);
    wc_to_utf8(d, buf, -1, BUFFER_SIZE);
}

static int out_wc(char *d, const WC *s, int n)
{
    WC buf[BUFFER_SIZE];

    zap_doublewidth(buf, s, n);
    return wc_to_mb(d, buf, n, OUTSTATE);
}

#undef TO_WC
#undef WRAP_WC
#undef OUT_WC
#define TO_WC(d, s) to_wc(d, s)
#define WRAP_WC(d, s) wrap_wc(d, s)
#define OUT_WC(d, s, n) out_wc(d, s, n)


#ifdef USER_DEBUG
static void debug_info(void)
{
    char txt[BUFFER_SIZE];
    sprintf(txt, "b_first=%d, b_current=%d, b_last=%d, b_bottom=%d, b_screenb=%d",
            b_first, b_current, b_last, b_bottom, b_screenb);
    tbuf+=sprintf(tbuf, "\033[1;%df\033[41;33;1m[\033[41;35m%s\033[41;33m]\033[37;40;0m",
            COLS-(int)strlen(txt)-1, txt);
    sprintf(txt, "k_len=%d, strlen(k_input)=%d, k_pos=%d, k_scrl=%d",
            k_len, (int)WClen(k_input), k_pos, k_scrl);
    tbuf+=sprintf(tbuf, "\033[2;%df\033[41;33;1m[\033[41;35m%s\033[41;33m]\033[37;40;0m",
            COLS-(int)strlen(txt)-1, txt);
}
#endif


static void redraw_cursor(void)
{
    tbuf+=sprintf(tbuf, "\033[%d;%df", scr_len+1, scr_curs+1);
}

static void countpos(void)
{
    k_pos=k_len=WClen(k_input);
    k_scrl=0;
}

/**********************/
/* rewrite input line */
/**********************/
static void redraw_in(void)
{
    int l, r, k;

    tbuf+=sprintf(tbuf, "\033[%d;1f%s", scr_len+1, input_color);
    if (k_pos<k_scrl)
        k_scrl=k_pos;
    if (k_pos-k_scrl+(k_scrl!=0)>=COLS)
        k_scrl=k_pos+2-COLS;
    if (k_scrl)
        tbuf+=sprintf(tbuf, "\033[1m<%s", input_color);
    if (retaining)
        tbuf+=sprintf(tbuf, "\033[30;1m");
    if (in_getpassword)
    {
        l=WClen(&(k_input[k_scrl]));
        l=(l<COLS-2+(k_scrl==0))?l:COLS-2+(k_scrl==0);
        for (int i=0;i<l;i++)
            *tbuf++='*';
    }
    else
    {
        l=COLS-2+(k_scrl==0);
        if (margins)
        {
            if (k_scrl+l>k_len)
                for (r=k_len-k_scrl;r<l;r++)
                    (k_input+k_scrl)[r]=' ';
            r=k_scrl;
            k=marginl-k_scrl-1;
            if (k>0)
            {
                tbuf+=OUT_WC(tbuf, k_input+r, k);
                r+=k;
                l-=k;
                k=0;
            }
            tbuf+=sprintf(tbuf, "\033[4%dm", MARGIN_COLOR_ANSI);
            k+=marginr-marginl+1;
            if (k>l)
                k=l;
            if (k>0)
            {
                tbuf+=OUT_WC(tbuf, k_input+r, k);
                r+=k;
                l-=k;
            }
            tbuf+=sprintf(tbuf, "%s", input_color);
            if (l>0)
                tbuf+=OUT_WC(tbuf, k_input+r, l);
            k_input[k_len]=0;
        }
        else
        {
            if (k_scrl+l>k_len)
                l=k_len-k_scrl;
            tbuf+=OUT_WC(tbuf, k_input+k_scrl, l);
        }
    }
    if (k_scrl+COLS-2<k_len)
        tbuf+=sprintf(tbuf, "\033[1m>");
    else if (!putty)
        tbuf+=sprintf(tbuf, "\033[0K");
    else
        for (int i=l;i<COLS-!!k_scrl;i++)
            *tbuf++=' ';
    scr_curs=(k_scrl!=0)+k_pos-k_scrl;
    redraw_cursor();
    term_commit();
}

/*************************/
/* write the status line */
/*************************/
static void redraw_status(void)
{
    const char *pos;
    int c, color=7;

    if (!isstatus)
        return;
    tbuf+=sprintf(tbuf, "\033[%d;1f\033[0;3%d;4%dm\033[2K\r", LINES,
                  STATUS_COLOR==0?7:0, STATUS_COLOR);
    if (!*(pos=status))
        goto end;
    while (*pos)
    {
        if (getcolor(&pos, &color, 0))
        {
            c=color;
            if (!(c&CBG_MASK))
                c=c|(STATUS_COLOR<<CBG_AT);
            if ((c&CFG_MASK) == (c&CBG_MASK) >> CBG_AT)
            {
                int k = c&CFG_MASK;
                if (k != 0 && k != 8)
                    k = 0;
                else if (STATUS_COLOR == 0)
                    k = 7;
                else
                    k = 0;
                c=(c&~CFG_MASK)|k;
            }
            tbuf=ansicolor(tbuf, c);
            pos++;
        }
        else
            one_utf8_to_mb(&tbuf, &pos, &outstate);
    }
end:
    redraw_cursor();
    term_commit();
}

static bool b_shorten(void)
{
    if (b_first>b_bottom)
        return false;
    SFREE(b_output[b_first%CONSOLE_LENGTH]);
    b_first++;
    return true;
}

/********************************************/
/* write a single line to the output window */
/********************************************/
static void draw_out(const char *pos)
{
    int c=7;
    while (*pos)
    {
        if (getcolor(&pos, &c, 0))
        {
            tbuf=ansicolor(tbuf, c);
            pos++;
            continue;
        }
        if (*pos=='\r') // wrapped line marker
        {
            pos++;
            continue;
        }
        one_utf8_to_mb(&tbuf, &pos, &outstate);
    }
}

/****************************/
/* scroll the output window */
/****************************/
static void b_scroll(int b_to)
{
    if (b_screenb==(b_to=(b_to<b_first?b_first:(b_to>b_bottom?b_bottom:b_to))))
        return;
    tbuf+=sprintf(tbuf, "\033[%d;%df\033[0;37;40m\033[1J", scr_len, COLS);
    term_commit();
    for (int y=b_to-LINES+(isstatus?3:2);y<=b_to;++y)
        if ((y>=1)&&(y>b_first))
        {
            tbuf+=sprintf(tbuf, "\033[%d;1f", scr_len+y-b_to);
            if (y<b_current)
                draw_out(b_output[y%CONSOLE_LENGTH]);
            else if (y==b_current)
                draw_out(out_line);
            else
                tbuf+=sprintf(tbuf, "\033[2K");
            term_commit();
        }
    tbuf+=sprintf(tbuf, "\0337");

    if (b_screenb==b_bottom)
    {
        tbuf+=sprintf(tbuf, "\033[%d;1f%s\033[1m\033[2K\033[?25l",
                scr_len+1, input_color);
        for (int x=0;x<COLS;++x)
            *tbuf++='^';
    }
    else
        tbuf+=sprintf(tbuf, "\033[?25h");
#ifdef USER_DEBUG
    debug_info();
#endif
    if ((b_screenb=b_to)==b_bottom)
        redraw_in();
    term_commit();
}

static void b_flush_newline(void)
{
    if (!b_pending_newline)
        return;
    tbuf+=sprintf(tbuf, "\033[0;37;40m\r\n\033[2K");
    b_pending_newline=0;
}

static void b_addline(void)
{
    b_flush_newline();
    b_pending_newline=1;
    char *new;
    while (!(new=MALLOC(o_len+1)))
        if (!b_shorten())
            die("Out of memory");
    out_line[o_len]=0;
    strcpy(new, out_line);
    if (b_bottom==b_first+CONSOLE_LENGTH)
        b_shorten();
    b_output[b_current%CONSOLE_LENGTH]=new;
    o_pos=0;
    o_len=setcolor(out_line, o_oldcolor=o_color);
    if (b_bottom<++b_current)
    {
        b_bottom=b_current;
        if (b_current==b_screenb+1)
        {
            ++b_screenb;
            term_commit();
        }
        else
            tbuf=term_buf;
    }
}

static void touch_bottom(void)
{
    if (b_bottom!=b_screenb)
        b_scroll(b_bottom);
}

/****************************************************/
/* textout - write some text into the output window */
/****************************************************/
static inline void print_char(const WC ch)
{
    int clen;

    if (ch==EMPTY_CHAR)
        return;
    /* wrap prematurely if the double-width char won't fit */
    int dw=wcwidth(ch);
    if (dw<0)
        dw=0;

    if (o_pos+dw-1>=COLS)
    {
        out_line[o_len++]='\r';
        b_addline();
        b_flush_newline();
        tbuf=ansicolor(tbuf, o_color);
    }
    else if (o_oldcolor!=o_color) /* b_addline already updates the color */
    {
        if ((o_color&CFG_MASK)==((o_color&CBG_MASK)>>CBG_AT))
            o_color=(o_color&~CFG_MASK)|((o_color&CFG_MASK)?0:7);
        o_len+=setcolor(out_line+o_len, o_color);
        if (b_screenb==b_bottom)
            tbuf=ansicolor(tbuf, o_color);
        o_oldcolor=o_color;
    }
    clen=wcrtomb(tbuf, ch, &outstate);
    if (clen!=-1)
        tbuf+=clen;
    else
        *tbuf++=translit(ch);
    o_len+=wc_to_utf8(out_line+o_len, &ch, 1, BUFFER_SIZE-8+term_buf-tbuf);
    o_pos+=dw;
}

static void form_feed(void)
{
    for (int i=(isstatus?2:1);i<LINES;i++)
        b_addline();
    tbuf+=sprintf(tbuf, "\033[f");
}

static void b_textout(const char *txt)
{
    wchar_t u[2];

    /* warning! terminal output can get discarded! */
    *tbuf++=27, *tbuf++='8';
    b_flush_newline();
    tbuf=ansicolor(tbuf, o_color);
    for (;*txt;txt++)
        switch (*txt)
        {
        case 27:    /* we're not supposed to see escapes here */
            print_char('^');
            print_char('[');
            break;
        case 7:
            write_stdout("\007", 1);
            break;
        case 8:
        case 127:
        case '\r':
            break;
        case '\n':
            out_line[o_len]=0;
            b_addline();
            break;
        case 9:
            if (o_pos<COLS) /* tabs on the right side of screen are ignored */
                do print_char(' '); while (o_pos&7);
            break;
        case 12:
            form_feed();
            break;
        case '~':
            if (getcolor(&txt, &o_color, 1))
            {
                if (o_color==-1)
                    o_color=o_prevcolor;
                break;
            }
            /* fall through */
        default:
            txt+=utf8_to_wc(u, txt, 1)-1;
            print_char(u[0]);
        }
    out_line[o_len]=0;
    tbuf+=sprintf(tbuf, "\0337");
#ifdef USER_DEBUG
    debug_info();
#endif
    redraw_cursor();
    if (b_screenb==b_bottom)
        term_commit();
    else
        tbuf=term_buf;
    o_prevcolor=o_color;
}

static void b_canceldraft(void)
{
    if (b_bottom==b_screenb)
    {
        tbuf+=sprintf(tbuf, "\0338\033[0m\033[2K");
        while (b_current>b_last)
        {
            b_current--;
            tbuf+=sprintf(tbuf, "\033[A\033[2K");
            assert(tbuf-term_buf < (ssize_t)sizeof(term_buf));
        }
        *tbuf++='\r';
        tbuf=ansicolor(tbuf, o_lastcolor);
        *tbuf++=27, *tbuf++='7';
    }
    else
        b_current=b_last;
    o_oldcolor=o_color=o_lastcolor;
    o_prevcolor=o_lastprevcolor;
    o_len=setcolor(out_line, o_lastcolor);
    o_pos=0;
}

static void usertty_textout(const char *txt)
{
    if (o_draftlen)
    {                  /* text from another source came, we need to either */
        if (o_strongdraft)
        {
            o_draftlen=0;                              /* accept the draft */
            if (lastdraft)
            {
                lastdraft->last_line[0]=0;
                lastdraft->lastintitle=0;
            }
            lastdraft=0;
        }
        else
            b_canceldraft();                  /* or temporarily discard it */
    }
    b_textout(txt);
    b_last=b_current;
    o_lastcolor=o_color;
    o_lastprevcolor=o_prevcolor;
    if (o_draftlen)
        b_textout(b_draft); /* restore the draft */
#ifdef USER_DEBUG
    debug_info();
    redraw_cursor();
    term_commit();
#endif
}

static void usertty_textout_draft(const char *txt, bool strong)
{
    if (o_draftlen)
        b_canceldraft();
    if (txt)
    {
        strcpy(b_draft, txt);
#ifdef USER_DEBUG
        strcat(b_draft, "\xe2\x96\xa0"); // ■ U+25A0 BLACK SQUARE
#endif
        if ((o_draftlen=strlen(b_draft)))
            b_textout(b_draft);
#ifdef USER_DEBUG
        debug_info();
        redraw_cursor();
        term_commit();
#endif
    }
    else
    {
        b_draft[0]=0;
        o_draftlen=0;
    }
    o_strongdraft=strong;
}

static void transpose_chars(void)
{
    WC w1[3], w2[3], *l, *r;

    w1[1]=w1[2]=w2[1]=w2[2]=0;
    if (k_input[k_pos-1]==EMPTY_CHAR)
    {
        if (k_pos==1)
            return;
        w1[0]=k_input[k_pos-2];
        w1[1]=EMPTY_CHAR;
    }
    else
        w1[0]=k_input[k_pos-1];
    if (k_pos<k_len-1 && k_input[k_pos+1]==EMPTY_CHAR)
    {
        w2[0]=k_input[k_pos];
        w2[1]=EMPTY_CHAR;
    }
    else
        w2[0]=k_input[k_pos];
    r=k_input+k_pos-WClen(w1);
    l=w2;
    while (*l)
        *r++=*l++;
    l=w1;
    while (*l)
        *r++=*l++;
    k_pos+=WClen(w2);
}

static void transpose_words(void)
{
    WC buf[BUFFER_SIZE];
    int a1, a2, b1, b2;

    a2=k_pos;
    while (a2<k_len && (k_input[a2]==EMPTY_CHAR || !iswalnum(k_input[a2])))
        a2++;
    if (a2==k_len)
        while (a2 && (k_input[a2-1]==EMPTY_CHAR || !iswalnum(k_input[a2-1])))
            a2--;
    while (a2 && (k_input[a2-1]==EMPTY_CHAR || iswalnum(k_input[a2-1])))
        a2--;
    if (k_input[a2]==EMPTY_CHAR)
        a2++;
    if (!a2)
        return;
    b2=a2;
    while (k_input[b2+1]==EMPTY_CHAR || iswalnum(k_input[b2+1]))
        b2++;
    b1=a2;
    do { if (--b1<0) return; } while (k_input[b1]==EMPTY_CHAR || !iswalnum(k_input[b1]));
    if (k_input[b1]==EMPTY_CHAR)
        b1++;
    a1=b1;
    while (a1>0 && (k_input[a1-1]==EMPTY_CHAR || iswalnum(k_input[a1-1])))
        a1--;
    if (k_input[a1]==EMPTY_CHAR)
        a1++;
    memcpy(buf, k_input+a2, (b2+1-a2)*WCL);
    memcpy(buf+b2+1-a2, k_input+b1+1, (a2-b1-1)*WCL);
    memcpy(buf+b2-b1, k_input+a1, (b1+1-a1)*WCL);
    memcpy(k_input+a1, buf, (b2+1-a1)*WCL);
    k_pos=b2+1;
}

static bool ret(bool r)
{
    if (!retaining)
        return false;
    if (!r)
    {
        k_len=k_pos=k_scrl=0;
        k_input[0]=0;
        scr_curs=0;
    }
    retaining=false;
    return true;
}

static void swap_inputs(void)
{
    WC buf[BUFFER_SIZE];
    ret(false);
    WCcpy(buf, k_input);
    WCcpy(k_input, tk_input);
    WCcpy(tk_input, buf);
    int i=k_pos; k_pos=tk_pos; tk_pos=i;
    i=k_scrl; k_scrl=tk_scrl; tk_scrl=i;
    i=k_len; k_len=tk_len; tk_len=i;
}

static enum
{
    TS_NORMAL,
    TS_ESC,
    TS_ESC_S,
    TS_ESC_S_S,
    TS_ESC_O,
    TS_ESC_S_G,
#if 0
    TS_VERBATIM,
#endif
} state=TS_NORMAL;
static int bits = 0;
#define MAXNVAL 10
static int val[MAXNVAL+1], nval;
static bool usertty_process_kbd(struct session *ses, WC ch)
{
    char txt[16];

    assert(ses);
#ifdef KEYBOARD_DEBUG
    if (ch==27)
        tintin_printf(ses, "~5~[~13~ESC~5~]~7~");
    else if (ch<=' ' || ch>=127)
        tintin_printf(ses, "~5~[~13~\\x%02x~5~]~7~", ch);
    else
        tintin_printf(ses, "~5~[~13~%c~5~]~7~", ch);
#endif

    switch (state)
    {
#if 0
    case TS_VERBATIM:
        state=TS_NORMAL;
        goto insert_verbatim;
        break;
#endif
    //-----------------------------------------------------------------------
    case TS_ESC_O:              /* ESC O */
        if (isadigit(ch))
        {
            val[nval]=val[nval]*10+(ch-'0');
            break;
        }
        else if (ch==';')
        {
            if (nval<MAXNVAL)
                val[++nval]=0;
            break;
        }
        if (val[0]>=2 && val[0]<=8) // modifier keys
            bits=val[0]-1;
        state=TS_NORMAL;
        if (!bits)
            switch (ch)
            {
            case 'A':
                goto prev_history;
            case 'B':
                goto next_history;
            case 'C':
                goto key_cursor_right;
            case 'D':
                goto key_cursor_left;
            case 'H':
                goto key_home;
            case 'F':
                goto key_end;
            }
        touch_bottom();
        sprintf(txt, "ESCO"WCC, (WCI)ch);
        find_bind(txt, bits, 1, ses);
        break;
    //-----------------------------------------------------------------------
    case TS_ESC_S_S:            /* ESC [ [ */
        state=TS_NORMAL;
        touch_bottom();
        sprintf(txt, "ESC[["WCC, (WCI)ch);
        find_bind(txt, bits, 1, ses);
        break;
    //-----------------------------------------------------------------------
    case TS_ESC_S:              /* ESC [ */
        if (isadigit(ch))
        {
            val[nval]=val[nval]*10+(ch-'0');
            break;
        }
        else if (ch==';')
        {
            if (nval<MAXNVAL)
                val[++nval]=0;
            break;
        }

        if (val[0] && val[1]>=2 && val[1]<=8) // modifier keys
            bits=val[1]-1;
        state=TS_NORMAL;
        if (ch>='A' && ch <='Z' && !bits)
        {
            switch (ch)
            {
            prev_history:
            case 'A':       /* up arrow */
                if (ret(false))
                    redraw_in();
                touch_bottom();
                if (hist_num==HISTORY_SIZE-1)
                    break;
                if (!history[hist_num+1])
                    break;
                if (hist_num==-1)
                    WCcpy(kh_input, k_input);
                TO_WC(k_input, history[++hist_num]);
                countpos();
                redraw_in();
                break;
            next_history:
            case 'B':       /* down arrow */
                if (ret(false))
                    redraw_in();
                touch_bottom();
                if (hist_num==-1)
                    break;
                do --hist_num;
                while ((hist_num>0)&&!(history[hist_num]));
                if (hist_num==-1)
                    WCcpy(k_input, kh_input);
                else
                    TO_WC(k_input, history[hist_num]);
                countpos();
                redraw_in();
                break;
            key_cursor_left:
            case 'D':       /* left arrow */
                touch_bottom();
                if (ret(true))
                   redraw_in();
                if (k_pos==0)
                    break;
                --k_pos;
                if (k_pos && k_input[k_pos]==EMPTY_CHAR)
                    --k_pos;
                if (k_pos>=k_scrl)
                {
                    scr_curs=k_pos-k_scrl+(k_scrl!=0);
                    redraw_cursor();
                    term_commit();
                }
                else
                    redraw_in();
                break;
            key_cursor_right:
            case 'C':       /* right arrow */
                touch_bottom();
                if (ret(true))
                   redraw_in();
                if (k_pos==k_len)
                    break;
                ++k_pos;
                if (k_pos<k_len && k_input[k_pos]==EMPTY_CHAR)
                    ++k_pos;
                if (k_pos<=k_scrl+COLS-2)
                {
                    scr_curs=k_pos-k_scrl+(k_scrl!=0);
                    redraw_cursor();
                    term_commit();
                }
                else
                    redraw_in();
                break;
            case 'H':
                goto key_home;
            case 'F':
                goto key_end;
            default:
                touch_bottom();
                sprintf(txt, "ESC["WCC, (WCI)ch);
                find_bind(txt, 0, 1, ses);
                break;
            }
        }
        else if (ch>='A' && ch <='Z') // && bits
        {
            touch_bottom();
            sprintf(txt, "ESC["WCC, (WCI)ch);
            find_bind(txt, bits, 1, ses);
        }
        else if (ch=='[')
            state=TS_ESC_S_S;
        else if (ch=='~' && !bits)
            switch (val[0])
            {
            case 5:         /* [PgUp] */
                if (b_screenb>b_first+LINES-(isstatus?3:2))
                    b_scroll(b_screenb+(isstatus?3:2)-LINES);
                else
                    write_stdout("\007", 1);
                break;
            case 6:         /* [PgDn] */
                if (b_screenb<b_bottom)
                    b_scroll(b_screenb+LINES-(isstatus?3:2));
                else
                    write_stdout("\007", 1);
                break;
            key_home:
            case 1:         /* [Home] */
            case 7:
                touch_bottom();
                if (ret(true))
                    redraw_in();
                if (!k_pos)
                    break;
                k_pos=0;
                if (k_pos>=k_scrl)
                {
                    scr_curs=k_pos-k_scrl+(k_scrl!=0);
                    redraw_cursor();
                    term_commit();
                }
                else
                    redraw_in();
                break;
            key_end:
            case 4:         /* [End] */
            case 8:
                touch_bottom();
                if (ret(true))
                    redraw_in();
                if (k_pos==k_len)
                    break;
                k_pos=k_len;
                if (k_pos<=k_scrl+COLS-2)
                {
                    scr_curs=k_pos-k_scrl+(k_scrl!=0);
                    redraw_cursor();
                    term_commit();
                }
                else
                    redraw_in();
                break;
            key_del:
            case 3:         /* [Del] */
                ret(false);
                touch_bottom();
                if (k_pos!=k_len)
                {
                    for (int i=k_pos;i<=k_len;++i)
                    k_input[i]=k_input[i+1];
                    --k_len;
                }
                if (k_pos!=k_len && k_input[k_pos]==EMPTY_CHAR)
                    goto key_del;
                redraw_in();
                break;
            default:
                touch_bottom();
                sprintf(txt, "ESC[%i~", val[0]);
                find_bind(txt, 0, 1, ses);
                break;
            }
        else if (ch=='~') // && bits
        {
            touch_bottom();
            sprintf(txt, "ESC[%i~", val[0]);
            find_bind(txt, bits, 1, ses);
        }
        else if (ch=='>')
            state=TS_ESC_S_G;
        break;
    //-----------------------------------------------------------------------
    case TS_ESC_S_G:            /* ESC [ > */
        if (isadigit(ch))
            val[nval]=val[nval]*10+(ch-'0');
        else if (ch==';')
        {
            if (nval<MAXNVAL)
                val[++nval]=0;
        }
        else if (ch=='c')
        {
            state=TS_NORMAL;
            /* answer from ESC [>c */
            bind_xterm((val[0]==0 && val[1]==115) /* konsole */
                       || val[0]==1               /* libvte, mlterm, aterm, termit */
                       || val[0]==41              /* xterm, terminology */
                       || val[0]==64              /* zutty */
                       || val[0]==83              /* screen */
                      );
        }
        else
            state=TS_NORMAL;
        break;
    //-----------------------------------------------------------------------
    case TS_ESC:                /* ESC */
        if (ch=='[')
        {
            state=TS_ESC_S; val[nval=0]=val[1]=0;
            break;
        }
        if (ch=='O')
        {
            state=TS_ESC_O; val[nval=0]=val[1]=0;
            break;
        }
        if (ch==27 && !bits)
        {
            bits = BIT_ALT;
            break;
        }
        state=TS_NORMAL;
        if (ch==127)
            sprintf(txt, "Alt-Backspace");
        else if ((unsigned char)ch>32)
            sprintf(txt, "Alt-"WCC, (WCI)ch);
        else if (ch==32)
            sprintf(txt, "Alt-Space");
#if 0
        else if (ch==27)
            sprintf(txt, "Alt-Esc");
#endif
        else if (ch==13)
            sprintf(txt, "Alt-Enter");
        else if (ch==9)
            sprintf(txt, "Alt-Tab");
        else
            sprintf(txt, "Alt-^"WCC, (WCI)(ch+64));
        if (find_bind(txt, 0, 0, ses)) // Alt- already included
            break;
        switch (ch)
        {
        case 9:         /* Alt-Tab */
            swap_inputs();
            redraw_in();
            break;
        case '<':       /* Alt-< */
            touch_bottom();
            if (ret(false))
                redraw_in();
            if (hist_num==HISTORY_SIZE-1 || !history[hist_num+1])
                break;
            do hist_num++;
                while (hist_num!=HISTORY_SIZE-1 && history[hist_num+1]);
            TO_WC(k_input, history[hist_num]);
            countpos();
            redraw_in();
            break;
        case '>':       /* Alt-> */
            touch_bottom();
            if (ret(false))
                redraw_in();
            if (hist_num==-1)
                break;
            hist_num=-1;
            WCcpy(k_input, kh_input);
            countpos();
            redraw_in();
            break;
        case 'f':   /* Alt-F */
        case 'F':
            touch_bottom();
            if (ret(true))
                redraw_in();
            if (k_pos==k_len)
                break;
            while (k_pos<k_len && (k_input[k_pos]==EMPTY_CHAR || !iswalnum(k_input[k_pos])))
                ++k_pos;
            while (k_pos<k_len && (k_input[k_pos]==EMPTY_CHAR || iswalnum(k_input[k_pos])))
                ++k_pos;
            if (k_pos<=k_scrl+COLS-2)
            {
                scr_curs=k_pos-k_scrl+(k_scrl!=0);
                redraw_cursor();
                term_commit();
            }
            else
                redraw_in();
            break;
        case 'b':   /* Alt-B */
        case 'B':
            touch_bottom();
            if (ret(true))
                redraw_in();
            if (!k_pos)
                break;
            while (k_pos && (k_input[k_pos]==EMPTY_CHAR || !iswalnum(k_input[k_pos-1])))
                --k_pos;
            while (k_pos && (k_input[k_pos]==EMPTY_CHAR || iswalnum(k_input[k_pos-1])))
                --k_pos;
            if (k_pos>=k_scrl)
            {
                scr_curs=k_pos-k_scrl+(k_scrl!=0);
                redraw_cursor();
                term_commit();
            }
            else
                redraw_in();
            break;
        case 'l':   /* Alt-L */
        case 'L':
            touch_bottom();
            ret(true);
            while (k_pos<k_len && (k_input[k_pos]==EMPTY_CHAR || !iswalnum(k_input[k_pos])))
                ++k_pos;
            while (k_pos<k_len && (k_input[k_pos]==EMPTY_CHAR || iswalnum(k_input[k_pos])))
            {
                if (k_input[k_pos]!=EMPTY_CHAR)
                    k_input[k_pos]=towlower(k_input[k_pos]);
                ++k_pos;
            }
            redraw_in();
            break;
        case 'u':   /* Alt-U */
        case 'U':
            touch_bottom();
            if (ret(true))
                redraw_in();
            while (k_pos<k_len && (k_input[k_pos]==EMPTY_CHAR || !iswalnum(k_input[k_pos])))
                ++k_pos;
            while (k_pos<k_len && (k_input[k_pos]==EMPTY_CHAR || iswalnum(k_input[k_pos])))
            {
                if (k_input[k_pos]!=EMPTY_CHAR)
                    k_input[k_pos]=towupper(k_input[k_pos]);
                ++k_pos;
            }
            redraw_in();
            break;
        case 'c':   /* Alt-C */
        case 'C':
            touch_bottom();
            ret(true);
            while (k_pos<k_len && (k_input[k_pos]==EMPTY_CHAR || !iswalnum(k_input[k_pos])))
                ++k_pos;
            if (k_pos<k_len && (k_input[k_pos]==EMPTY_CHAR || iswalnum(k_input[k_pos])))
            {
                if (k_input[k_pos]!=EMPTY_CHAR)
                    k_input[k_pos]=towupper(k_input[k_pos]);
                ++k_pos;
            }
            while (k_pos<k_len && (k_input[k_pos]==EMPTY_CHAR || iswalnum(k_input[k_pos])))
            {
                if (k_input[k_pos]!=EMPTY_CHAR)
                    k_input[k_pos]=towlower(k_input[k_pos]);
                ++k_pos;
            }
            redraw_in();
            break;
        case 't':   /* Alt-T */
        case 'T':
            touch_bottom();
            ret(true);
            transpose_words();
            redraw_in();
            break;
        case 'd':  /* Alt-D */
        case 'D':
            if (ret(false))
                redraw_in();
            touch_bottom();
            if (k_pos==k_len)
                break;
            {
                int i=k_pos;
                while (i<k_len && (k_input[i]==EMPTY_CHAR || !iswalnum(k_input[i])))
                    i++;
                while (k_input[i]==EMPTY_CHAR || iswalnum(k_input[i]))
                    i++;
                i-=k_pos;
                memcpy(yank_buffer, k_input+k_pos, i*WCL);
                yank_buffer[i]=0;
                memmove(k_input+k_pos, k_input+k_pos+i, (k_len-k_pos-i)*WCL);
                k_len-=i;
                k_input[k_len]=0;
                redraw_in();
            }
            break;
        case 127:  /* Alt-Backspace */
            touch_bottom();
            ret(false);
            if (k_pos)
            {
                int i=k_pos-1;
                while ((i>=0)&&(k_input[i]==EMPTY_CHAR || !iswalnum(k_input[i])))
                    i--;
                while ((i>=0)&&(k_input[i]==EMPTY_CHAR || iswalnum(k_input[i])))
                    i--;
                i=k_pos-i-1;
                memmove(yank_buffer, k_input+k_pos-i, i*WCL);
                yank_buffer[i]=0;
                memmove(k_input+k_pos-i, k_input+k_pos, (k_len-k_pos)*WCL);
                k_len-=i;
                k_input[k_len]=0;
                k_pos-=i;
            }
            redraw_in();
            break;
        default:
            find_bind(txt, 0, 1, ses); /* FIXME: we want just the message */
        }
        break;
    //-----------------------------------------------------------------------
    case TS_NORMAL:
        bits=0;
        switch (ch)
        {
        case '\n':
        case '\r':
            ret(true);
            touch_bottom();
            if (!activesession->server_echo)
                in_getpassword=false;
            WRAP_WC(done_input, k_input);
            if (retain)
            {
                k_pos=k_len;
                retaining=true;
            }
            else
            {
                k_len=k_pos=k_scrl=0;
                k_input[0]=0;
                scr_curs=0;
            }
            redraw_in();
#if 0
            tbuf+=sprintf(tbuf, "\033[%d;1f%s\033[2K", scr_len+1, input_color);
            if (margins&&(marginl<=COLS))
            {
                tbuf+=sprintf(tbuf, "\033[%d;%df\033[4%dm",
                              scr_len+1, marginl, MARGIN_COLOR_ANSI);
                if (marginr<=COLS)
                    i=marginr+1-marginl;
                else
                    i=COLS+1-marginl;
                while (i--)
                    *tbuf++=' ';
                tbuf+=sprintf(tbuf, "%s\033[1m\033[%d;1f", input_color, scr_len+1);
            }
            scr_curs=0;
            term_commit();
#endif
            return true;
        case 1:                 /* ^[A] */
            if (find_bind("^A", 0, 0, ses))
                break;
            goto key_home;
        case 2:                 /* ^[B] */
            if (find_bind("^B", 0, 0, ses))
                break;
            goto key_cursor_left;
        case 4:                 /* ^[D] */
            if (find_bind("^D", 0, 0, ses))
                break;
            if (k_pos||k_len)
                goto key_del;
            if (ret(false))
                redraw_in();
            *done_input=0;
            activesession=zap_command("", ses);
            return false;
        case 5:                 /* ^[E] */
            if (find_bind("^E", 0, 0, ses))
                break;
            goto key_end;
        case 6:                 /* ^[F] */
            if (find_bind("^F", 0, 0, ses))
                break;
            goto key_cursor_right;
        case 8:                 /* ^[H] */
            if (find_bind("^H", 0, 0, ses))
                break;
        case 127:               /* [backspace] */
            touch_bottom();
            ret(false);
            if (k_pos)
            {
                int dw=(k_input[--k_pos]==EMPTY_CHAR)?2:1;
                if (dw==2)
                    --k_pos;
                for (int i=k_pos;i<=k_len;++i)
                    k_input[i]=k_input[i+dw];
                k_len-=dw;
            }
            redraw_in();
            break;
        case 9:                 /* [Tab], ^[I] */
            if (find_bind("Tab", 0, 0, ses)||find_bind("^I", 0, 0, ses))
                break;
            swap_inputs();
            redraw_in();
            break;
        case 11:                /* ^[K] */
            if (find_bind("^K", 0, 0, ses))
                break;
            ret(false);
            touch_bottom();
            if (k_pos!=k_len)
            {
                memmove(yank_buffer, k_input+k_pos, (k_len-k_pos)*WCL);
                yank_buffer[k_len-k_pos]=0;
                k_input[k_pos]=0;
                k_len=k_pos;
            }
            redraw_in();
            break;
        case 14:                /* ^[N] */
            if (find_bind("^N", 0, 0, ses))
                break;
            goto next_history;
        case 16:                /* ^[P] */
            if (find_bind("^P", 0, 0, ses))
                break;
            goto prev_history;
#if 0
        case 17:                /* ^[Q] */
            if (find_bind("^Q", 0, 0, ses))
                break;
            state=TS_VERBATIM;
            break;
#endif
        case 20:                /* ^[T] */
            if (find_bind("^T", 0, 0, ses))
                break;
            ret(true);
            touch_bottom();
            if (k_pos && (k_len>=2))
            {
                if (k_pos==k_len)
                    if (k_input[--k_pos]==EMPTY_CHAR)
                        --k_pos;
                transpose_chars();
            }
            redraw_in();
            break;
        case 21:                /* ^[U] */
            if (find_bind("^U", 0, 0, ses))
                break;
            ret(false);
            touch_bottom();
            if (k_pos)
            {
                memmove(yank_buffer, k_input, k_pos*WCL);
                yank_buffer[k_pos]=0;
                memmove(k_input, k_input+k_pos, (k_len-k_pos+1)*WCL);
                k_len-=k_pos;
                k_pos=k_scrl=0;
            }
            redraw_in();
            break;
#if 0
        case 22:                /* ^[V] */
            if (find_bind("^V", 0, 0, ses))
                break;
            state=TS_VERBATIM;
            break;
#endif
        case 23:                /* ^[W] */
            if (find_bind("^W", 0, 0, ses))
                break;
            ret(false);
            touch_bottom();
            if (k_pos)
            {
                int i=k_pos-1;
                while ((i>=0)&&(k_input[i]==EMPTY_CHAR || iswspace(k_input[i])))
                    i--;
                while ((i>=0)&&(k_input[i]==EMPTY_CHAR || !iswspace(k_input[i])))
                    i--;
                i=k_pos-i-1;
                memmove(yank_buffer, k_input+k_pos-i, i*WCL);
                yank_buffer[i]=0;
                memmove(k_input+k_pos-i, k_input+k_pos, (k_len-k_pos)*WCL);
                k_len-=i;
                k_input[k_len]=0;
                k_pos-=i;
            }
            redraw_in();
            break;
        case 25:                /* ^[Y] */
            if (find_bind("^Y", 0, 0, ses))
                break;
            ret(false);
            touch_bottom();
            if (!*yank_buffer)
            {
                redraw_in();
                write_stdout("\007", 1);
                break;
            }
            {
                int i=WClen(yank_buffer);
                if (i+k_len>=BUFFER_SIZE)
                    i=BUFFER_SIZE-1-k_len;
                memmove(k_input+k_pos+i, k_input+k_pos, (k_len-k_pos)*WCL);
                memmove(k_input+k_pos, yank_buffer, i*WCL);
                k_len+=i;
                k_input[k_len]=0;
                k_pos+=i;
                redraw_in();
            }
            break;
        case 27:        /* [Esc] or a control sequence */
            state=TS_ESC;
            break;
        case EMPTY_CHAR:
            break;
        default:
            if (ret(false))
                redraw_in();
            touch_bottom();
            if ((ch>0)&&(ch<32))
            {
                sprintf(txt, "^"WCC, (WCI)(ch+64));
                find_bind(txt, 0, 1, ses);
                break;
            }
#if 0
        insert_verbatim:
#endif
            int dw=isw2width(ch)?2:1;
            if (k_len+dw==BUFFER_SIZE)
                write_stdout("\007", 1);
            else
            {
                k_input[k_len+=dw]=0;
                for (int i=k_len-dw;i>k_pos;--i)
                    k_input[i]=k_input[i-dw];
                k_input[k_pos++]=ch;
                if (dw==2)
                    k_input[k_pos++]=EMPTY_CHAR;
                if ((k_len==k_pos)&&(k_len<COLS))
                {
                    scr_curs+=dw;
                    tbuf+=sprintf(tbuf, "%s", input_color);
                    if (margins && (k_len>=marginl)&&(k_len<=marginr))
                        tbuf+=sprintf(tbuf, "\033[4%dm", MARGIN_COLOR_ANSI);
                    if (in_getpassword)
                    {
                        *tbuf++='*';
                        if (dw==2)
                            *tbuf++='*';
                    }
                    else
                        tbuf+=OUT_WC(tbuf, &ch, 1);
                    term_commit();
                }
                else
                    redraw_in();
            }
        }
    }
#ifdef USER_DEBUG
    debug_info();
    redraw_cursor();
    term_commit();
#endif
    return false;
}

/********************************************/
/* reformat the scrollback into a new width */
/********************************************/
static void b_resize(void)
{
    char *src[CONSOLE_LENGTH];
    int src_lines;
    char line[BUFFER_SIZE], *lp;
    int cont;
    int color;

    src_lines=b_bottom-b_first;
    if (term_width==COLS)
        return;
    term_width=COLS;

    assert(src_lines<=CONSOLE_LENGTH);
    if (b_bottom%CONSOLE_LENGTH > b_first%CONSOLE_LENGTH)
        memcpy(src, b_output+(b_first%CONSOLE_LENGTH), src_lines*sizeof(char*));
    else
    {
        memcpy(src, b_output+(b_first%CONSOLE_LENGTH), (CONSOLE_LENGTH-b_first%CONSOLE_LENGTH)*sizeof(char*));
        memcpy(src+CONSOLE_LENGTH-b_first%CONSOLE_LENGTH, b_output, (b_bottom%CONSOLE_LENGTH)*sizeof(char*));
    }
    o_len=o_pos=0;
    o_oldcolor=o_prevcolor=o_lastprevcolor=7;
    if (b_greeting>=b_first)
        b_greeting-=b_first;
    b_bottom=b_current=b_last=b_first=0;
    lp=line;
    cont=0;
    color=-1;
    for (int i=0;i<src_lines;i++)
    {
        int ncolor=color;
        const char *sp=src[i];
        if (cont && *sp=='~' && getcolor(&sp, &ncolor, 0))
        {
            /* don't insert extra color codes for continuations */
            if (color!=ncolor)
                sp=src[i];
        }
        while (*sp && lp-line<BUFFER_SIZE-2)
            *lp++=*sp++;
        SFREE(src[i]);
        if ((cont=(lp>line && lp[-1]=='\r')))
            lp--;
        else
        {
            *lp++='\n';
            *lp=0;
            b_textout(line);
            lp=line;
        }
    }
    if (cont)
    {
        *lp++='\n';
        *lp=0;
        b_textout(line);
    }
    b_last=b_current;
    if (o_draftlen)
        b_textout(b_draft); /* restore the draft */
}

static void usertty_mark_greeting(void)
{
    b_greeting=b_bottom;
}

/******************************/
/* set up the window outlines */
/******************************/
static void usertty_drawscreen(void)
{
    need_resize=false;
    scr_len=LINES-1-isstatus;
    tbuf+=sprintf(tbuf, "\033[0;37;40m\033[2J\033[0;37;40m\033[1;%dr\0337", scr_len);
    tbuf+=sprintf(tbuf, "\033[%d;1f%s", scr_len+1, input_color);
    if (!putty)
        tbuf+=sprintf(tbuf, "\033[2K");
    else
        for (int i=0;i<COLS;i++)
            *tbuf++=' ';
    if (isstatus)
        tbuf+=sprintf(tbuf, "\033[%d;f\033[37;4%dm\033[2K", LINES, STATUS_COLOR);
}

static void usertty_keypad(bool k)
{
    /*
    Force gnome-terminal to its vt220 mode, as it will ignore the keypad mode
    otherwise.  It seems to not hurt any other terminal I checked.
    */
    if (k)
        tbuf+=sprintf(tbuf, "\033=\033[?1051l\033[?1052l\033[?1060l\033[?1061h");
    else
        tbuf+=sprintf(tbuf, "\033>\033[?1051l\033[?1052l\033[?1060l\033[?1061l");
    term_commit();
}

static void usertty_retain(void)
{
    ret(false);
    redraw_in();
}

/*********************/
/* resize the screen */
/*********************/
static void usertty_resize(void)
{
    term_commit();
    term_getsize();
    if (term_width!=COLS)
        b_resize();
    usertty_drawscreen();
    b_screenb=-666;             /* impossible value */
    b_scroll(b_bottom);
    if (isstatus)
        redraw_status();
    redraw_in();

    telnet_resize_all();
}

static void usertty_show_status(void)
{
    bool st;
    st=!!strcmp(status, EMPTY_LINE);
    if (st!=isstatus)
    {
        isstatus=st;
        usertty_resize();
    }
    else if (st)
    {
        redraw_status();
        redraw_cursor();
        term_commit();
    }
}

static void usertty_init(void)
{
    char* term;

    ZERO(outstate);
#ifdef XTERM_TITLE
    xterm=getenv("DISPLAY")&&(getenv("WINDOWID")||getenv("KONSOLE_DCOP_SESSION"));
#endif
    /* screen's support for bg colors is bad */
    putty=(term=getenv("TERM"))&&!strncasecmp(term, "screen", 6);
    term_getsize();
    term_width=COLS;
    term_init();
    tbuf=term_buf+sprintf(term_buf, "\033[?7l");
    usertty_keypad(keypad);
    isstatus=false;
    retaining=false;
    usertty_drawscreen();

    margins=false;
    marginl=
        k_len=0;
    k_pos=0;
    k_scrl=0;
    k_input[0]=0;
    tk_len=0;
    tk_pos=0;
    tk_scrl=0;
    tk_input[0]=0;
    in_getpassword=false;

    b_first=0;
    b_current=0;
    b_bottom=0;
    b_screenb=0;
    b_last=-1;
    b_output[0]=out_line;
    out_line[0]=0;
    o_len=0;
    o_pos=0;
    o_color=7;
    o_oldcolor=7;
    o_prevcolor=7;
    o_lastprevcolor=7;
    o_draftlen=0;
    o_strongdraft=false;
    o_lastcolor=7;

    tbuf+=sprintf(tbuf, "\033[1;1f\0337");
    tbuf+=sprintf(tbuf, "\033[>c"); /* query the terminal type */

    sprintf(done_input, "~12~KB~3~tin ~7~%s by ~11~kilobyte@angband.pl~9~\n", VERSION);
    usertty_textout(done_input);
    for (int i=0;i<COLS;++i)
        done_input[i]='-';
    sprintf(done_input+COLS, "~7~\n");
    usertty_textout(done_input);
}

static void usertty_done(void)
{
    ui_own_output=false;
    b_flush_newline();
    tbuf+=sprintf(tbuf, "\033[1;%dr\033[%d;1f\033[?25h\033[?7h\033[0;37;40m", LINES, LINES);
    usertty_keypad(false);
    term_commit();
    term_restore();
    write_stdout("\n", 1);
#ifdef HAVE_VALGRIND_VALGRIND_H
    if (RUNNING_ON_VALGRIND)
    {
        while (b_shorten())
            ;
    }
#endif
}

static void usertty_pause(void)
{
    usertty_keypad(false);
    b_flush_newline();
    tbuf+=sprintf(tbuf, "\033[1;%dr\033[%d;1f\033[?25h\033[?7h\033[0;37;40m", LINES, LINES);
    term_commit();
    term_restore();
}

static void usertty_resume(void)
{
    term_getsize();
    term_init();
    tbuf=term_buf+sprintf(term_buf, "\033[?7l");
    usertty_keypad(keypad);
    usertty_drawscreen();
    b_screenb=-666;     /* impossible value */
    if (term_width!=COLS)
        b_resize();
    b_scroll(b_bottom);
    if (isstatus)
        redraw_status();
    redraw_in();
}


static bool fwrite_out(FILE *f, const char *pos)
{
    int c=dump_color;
    bool eol=true;
    char lstr[BUFFER_SIZE], ustr[BUFFER_SIZE], *s=ustr;

    for (;*pos;pos++)
    {
        if (*pos=='~')
            if (getcolor(&pos, &c, 0))
            {
                if (c==dump_color)
                    continue;
                s=ansicolor(s, c);
                dump_color=c;
                continue;
            }
        if (*pos=='\r')
            eol=false;
        else if (*pos!='\n')
            *s++=*pos;
    }
    *s=0;
    utf8_to_local(lstr, ustr);
    fputs(lstr, f);
    return eol;
}

static void usertty_condump(FILE *f)
{
    dump_color=7;
    for (int i = (b_greeting>b_first)?b_greeting:b_first; i<b_current; i++)
        if (fwrite_out(f, b_output[i%CONSOLE_LENGTH]))
            fprintf(f, "\n");
    fwrite_out(f, out_line);
}

static void usertty_passwd(bool x)
{
    ret(false);
    in_getpassword=x;
    redraw_in();
}

static void usertty_title(const char *fmt, ...)
{
#ifdef XTERM_TITLE
    va_list ap;
    char buf[BUFFER_SIZE];
    if (!xterm)
        return;

    va_start(ap, fmt);
    if (vsnprintf(buf, BUFFER_SIZE-1, fmt, ap)>BUFFER_SIZE-2)
        buf[BUFFER_SIZE-3]='>';
    va_end(ap);

    tbuf+=sprintf(tbuf, "\033]0;");
    utf8_to_mb(&tbuf, buf, &outstate);
    *tbuf++='\007';
    term_commit();
#endif
}

static void usertty_beep(void)
{
    write_stdout("\007", 1);
}

static void usertty_input_color(int c)
{
    ansicolor(input_color, c);
}


void usertty_initdriver(void)
{
    ansicolor(input_color, INPUT_COLOR);

    ui_sep_input=true;
    ui_con_buffer=true;
    ui_keyboard=true;
    ui_own_output=true;
    ui_drafts=true;

    user_init           = usertty_init;
    user_done           = usertty_done;
    user_pause          = usertty_pause;
    user_resume         = usertty_resume;
    user_textout        = usertty_textout;
    user_textout_draft  = usertty_textout_draft;
    user_process_kbd    = usertty_process_kbd;
    user_beep           = usertty_beep;
    user_keypad         = usertty_keypad;
    user_retain         = usertty_retain;
    user_passwd         = usertty_passwd;
    user_condump        = usertty_condump;
    user_title          = usertty_title;
    user_resize         = usertty_resize;
    user_show_status    = usertty_show_status;
    user_mark_greeting  = usertty_mark_greeting;
    user_input_color    = usertty_input_color;
}
