#include "tintin.h"
#include "protos/glob.h"
#include "protos/globals.h"
#include "protos/eval.h"
#include "protos/math.h"
#include "protos/print.h"
#include "protos/parse.h"
#include "protos/utils.h"
#include "protos/vars.h"


extern session *if_command(const char *arg, session *ses);

static void addroute(session *ses, int a, int b, char *way, num_t dist, char *cond)
{
    auto R = ses->routes[a].begin();
    while (R != ses->routes[a].end() && R->dest != b)
        ++R;
    if (R!=ses->routes[a].end())
    {
        auto &r = *R;
        r.path = way;
        r.distance = dist;
        r.cond = cond;
    }
    else
    {
        auto &r = ses->routes[a].emplace_back();
        r.dest = b;
        r.path = way;
        r.distance = dist;
        r.cond = cond;
    }
}

int count_routes(session *ses)
{
    int num=0;

    for (size_t i = 0; i < ses->locations.size(); ++i)
        for (auto&& r [[maybe_unused]] : ses->routes[i])
            num++;
    return num;
}

static void kill_unused_locations(session *ses)
{
    size_t n = ses->locations.size();
    std::vector<bool> us(n);

    for (unsigned i=0; i<n; i++)
        if (!ses->routes[i].empty())
        {
            us[i] = true;
            for (auto&& r : ses->routes[i])
                us[r.dest]=true;
        }
    for (unsigned i=0; i<n; i++)
        if ((!ses->locations[i].empty()) && !us[i])
            ses->locations[i].clear();
}

static void show_route(session *ses, int a, routenode *r)
{
    char num[32];
    num2str(num, r->distance);

    if (!r->cond.empty())
        tintin_printf(ses, "~7~{%s~7~}->{%s~7~}: {%s~7~} d=%s if {%s~7~}",
            ses->locations[a].c_str(),
            ses->locations[r->dest].c_str(),
            r->path.c_str(),
            num,
            r->cond.c_str());
    else
        tintin_printf(ses, "~7~{%s~7~}->{%s~7~}: {%s~7~} d=%s",
            ses->locations[a].c_str(),
            ses->locations[r->dest].c_str(),
            r->path.c_str(),
            num);
}

/***********************/
/* the #route command  */
/***********************/
void route_command(const char *arg, session *ses)
{
    char a[BUFFER_SIZE], b[BUFFER_SIZE], way[BUFFER_SIZE], dist[BUFFER_SIZE], cond[BUFFER_SIZE];
    num_t d;
    int n=ses->locations.size();

    arg=get_arg(arg, a, 0, ses);
    arg=get_arg(arg, b, 0, ses);
    arg=get_arg_in_braces(arg, way, 0);
    arg=get_arg(arg, dist, 0, ses);
    arg=get_arg_in_braces(arg, cond, 1);
    if (!*a)
    {
        tintin_printf(ses, "#THESE ROUTES HAVE BEEN DEFINED:");
        for (int i=0;i<n;i++)
            for (auto&& r : ses->routes[i])
                show_route(ses, i, &r);
        return;
    }
    if (!*way)
    {
        bool first=true;
        if (!*b)
            strcpy(b, "*");
        for (int i=0;i<n;i++)
            if ((!ses->locations[i].empty()) && match(a, ses->locations[i].c_str()))
                for (auto&& r : ses->routes[i])
                    if ((!ses->locations[i].empty()) &&
                          match(b, ses->locations[r.dest].c_str()))
                    {
                        if (first)
                        {
                            tintin_printf(ses, "#THESE ROUTES HAVE BEEN DEFINED:");
                            first=false;
                        }
                        show_route(ses, i, &r);
                    }
        if (first)
            tintin_printf(ses, "#THAT ROUTE (%s) IS NOT DEFINED.", b);
        return;
    }
    int i;
    for (i=0;i<n;i++)
        if ((!ses->locations[i].empty()) && ses->locations[i] == a)
            goto found_i;
    if (i==n)
    {
        for (i=0;i<n;i++)
            if (ses->locations[i].empty())
            {
                ses->locations[i] = a;
                goto found_i;
            }

        ses->locations.emplace_back(a);
        ses->routes.emplace_back();
        n++;
    }
found_i:;
    int j;
    for (j=0;j<n;j++)
        if ((!ses->locations[j].empty()) && ses->locations[j] == b)
            goto found_j;
    if (j==n)
    {
        for (j=0;j<n;j++)
            if (ses->locations[j].empty())
            {
                ses->locations[j] = b;
                goto found_j;
            }

        ses->locations.emplace_back(b);
        ses->routes.emplace_back();
        n++;
    }
found_j:
    if (*dist)
    {
        char *err;
        d=str2num(dist, &err);
        if (*err)
        {
            tintin_eprintf(ses, "#Hey! Route length has to be a number! Got {%s}.", arg);
            kill_unused_locations(ses);
            return;
        }
        if ((d<0)&&(ses->mesvar[MSG_ROUTE]||ses->mesvar[MSG_ERROR]))
            tintin_eprintf(ses, "#Error: distance cannot be negative!");
    }
    else
        d=DEFAULT_ROUTE_DISTANCE;
    addroute(ses, i, j, way, d, cond);
    if (ses->mesvar[MSG_ROUTE])
    {
        char num[32];
        num2str(num, d);

        if (*cond)
            tintin_printf(ses, "#Ok. Way from {%s} to {%s} is now set to {%s} (distance=%s) condition:{%s}",
                ses->locations[i].c_str(),
                ses->locations[j].c_str(),
                way,
                num,
                cond);
        else
            tintin_printf(ses, "#Ok. Way from {%s} to {%s} is now set to {%s} (distance=%s)",
                ses->locations[i].c_str(),
                ses->locations[j].c_str(),
                way,
                num);
    }
    routnum++;
}

/*************************/
/* the #unroute command  */
/*************************/
void unroute_command(const char *arg, session *ses)
{
    char a[BUFFER_SIZE], b[BUFFER_SIZE];
    bool found=false;

    arg=get_arg(arg, a, 0, ses);
    arg=get_arg(arg, b, 1, ses);

    if (!*a)
    {
        tintin_eprintf(ses, "#SYNTAX: #unroute <from> <to>");
        return;
    }

    int n = ses->locations.size();
    for (int i=0;i<n;i++)
    {
        if (ses->locations[i].empty())
            continue;
        bool is_a = match(a, ses->locations[i].c_str());

        if (is_a || !*b)
            ses->routes[i].remove_if([&](routenode &r)
            {
                if (*b ? match(b, ses->locations[r.dest].c_str())
                       : is_a || match(a, ses->locations[r.dest].c_str()))
                {
                    tintin_printf(MSG_ROUTE, ses,
                        "#Ok. There is no longer a route from {%s~-1~} to {%s~-1~}.",
                        ses->locations[i].c_str(),
                        ses->locations[r.dest].c_str());
                    found=true;
                    return true;
                }
                else
                    return false;
            });
    }
    if (found)
        kill_unused_locations(ses);
    else
        tintin_printf(MSG_ROUTE, ses, "#THAT ROUTE (%s) IS NOT DEFINED.", b);
}

#define INF 0x7FFFFFFFffffffffLL

/**********************/
/* the #goto command  */
/**********************/
void goto_command(const char *arg, session *ses)
{
    int n=ses->locations.size();
    char Astr[BUFFER_SIZE], Bstr[BUFFER_SIZE], cond[BUFFER_SIZE];
    int a, b, i, j;
    num_t s;
    std::vector<num_t> d(n);
    std::vector<int> ok(n);
    std::vector<int> way(n);
    std::vector<std::string> path(n);
    std::vector<std::string> locs(n);

    arg=get_arg(arg, Astr, 0, ses);
    arg=get_arg(arg, Bstr, 1, ses);

    if (!*Astr || !*Bstr)
    {
        tintin_eprintf(ses, "#SYNTAX: #goto <from> <to>");
        return;
    }

    const std::string A(Astr);
    const std::string B(Bstr);

    for (a = 0; a < n; a++)
        if (ses->locations[a] == A)
            break;
    if (a == n)
    {
        tintin_eprintf(ses, "#Location not found: [%s]", Astr);
        return;
    }
    for (b = 0; b < n; b++)
        if (ses->locations[b] == B)
            break;
    if (b == n)
    {
        tintin_eprintf(ses, "#Location not found: [%s]", Bstr);
        return;
    }
    for (i=0;i<n;i++)
    {
        d[i]=INF;
        ok[i]=0;
    }
    d[a]=0;
    do
    {
        s=INF;
        for (j=0;j<n;j++)
            if (!ok[j]&&(d[j]<s))
                s=d[i=j];
        if (s==INF)
        {
            tintin_eprintf(ses, "#No route from %s to %s!", Astr, Bstr);
            return;
        }
        ok[i]=1;
        for (auto&& r : ses->routes[i])
            if (d[r.dest]>s+r.distance)
            {
                if (r.cond.empty())
                    goto good;
                substitute_vars(r.cond.c_str(), cond, ses);
                if (eval_expression(cond, ses))
                {
                good:
                    d[r.dest]=s+r.distance;
                    way[r.dest]=i;
                }
            }
    } while (!ok[b]);
    j=0;
    for (i=b;i!=a;i=way[i])
        d[j++]=i;
    for (d[i=j]=a;i>0;i--)
    {
        locs[i] = ses->locations[d[i]];
        for (auto&& r : ses->routes[d[i]])
            if (r.dest==d[i-1])
                path[i] = r.path;
    }

    /*
       we need to copy all used route data (paths and location names)
       because of ugly bad users who can use #unroute in the middle
       of a #go command
    */
    locs[0] = B;
    for (i=j;i>0;i--)
    {
        tintin_printf(MSG_GOTO, ses, "#going from %s to %s", locs[i].c_str(), locs[i-1].c_str());
        parse_input(path[i].c_str(), true, ses);
    }
    set_variable("loc", Bstr, ses);
}

/************************/
/* the #dogoto command  */
/************************/
session * dogoto_command(const char *arg, session *ses)
{
    int n=ses->locations.size();
    char Astr[BUFFER_SIZE], Bstr[BUFFER_SIZE],
        distvar[BUFFER_SIZE], locvar[BUFFER_SIZE], pathvar[BUFFER_SIZE];
    char tmp[BUFFER_SIZE], cond[BUFFER_SIZE];
    int a, b, i, j;
    num_t s;
    std::vector<num_t> d(n);
    std::vector<int> ok(n);
    std::vector<int> way(n);
    char path[BUFFER_SIZE], *pptr;

    arg=get_arg(arg, Astr, 0, ses);
    arg=get_arg(arg, Bstr, 0, ses);
    arg=get_arg(arg, distvar, 0, ses);
    arg=get_arg(arg, locvar, 0, ses);
    arg=get_arg(arg, pathvar, 0, ses);

    if ((!*Astr) || (!*Bstr))
    {
        tintin_eprintf(ses, "#SYNTAX: #dogoto <from> <to> [<distvar> [<locvar> [<pathvar>]]] [#else ...]");
        return ses;
    }
    bool flag=*distvar||*locvar||*pathvar;

    std::string A(Astr);
    std::string B(Bstr);

    for (a = 0; a < n; a++)
        if (ses->locations[a] == A)
            break;
    if (a == n)
        goto not_found;
    for (b = 0; b < n; b++)
        if (ses->locations[b] == B)
            break;
    if (b == n)
        goto not_found;
    for (i=0;i<n;i++)
    {
        d[i]=INF;
        ok[i]=0;
    }
    d[a]=0;
    do
    {
        s=INF;
        for (j=0;j<n;j++)
            if (!ok[j]&&(d[j]<s))
                s=d[i=j];
        if (s==INF)
            goto not_found;
        ok[i]=1;
        for (auto&& r : ses->routes[i])
            if (d[r.dest] > s+r.distance)
            {
                if (r.cond.empty())
                    goto good;
                substitute_vars(r.cond.c_str(), cond, ses);
                if (eval_expression(cond, ses))
                {
                good:
                    d[r.dest] = s + r.distance;
                    way[r.dest] = i;
                }
            }
    } while (!ok[b]);
    num2str(tmp, d[b]);
    if (*distvar)
        set_variable(distvar, tmp, ses);
    j=0;
    for (i=b;i!=a;i=way[i])
        d[j++]=i;
    d[j]=a;
    pptr=path;
    for (i = j; i >= 0; i--)
    {
        pptr+=snprintf(pptr, path-pptr+BUFFER_SIZE-1, " %s", ses->locations[d[i]].c_str());
        if (pptr>=path+BUFFER_SIZE-2)
            break;
    }
    pptr=path+(pptr!=path);
    if (*locvar)
        set_variable(locvar, pptr, ses);
    pptr=path;
    for (i=j;i>0;i--)
    {
        for (auto&& r : ses->routes[d[i]])
            if (r.dest == d[i-1])
            {
                if (flag)
                {
                    pptr+=snprintf(pptr,
                        path-pptr+BUFFER_SIZE-1,
                        " {%s}",
                        r.path.c_str());
                    if (pptr>=path+BUFFER_SIZE-2)
                    {
                        tintin_eprintf(ses, "#Path too long in #dogoto");
                        goto truncated_path;
                    }
                }
                else
                {
                    tintin_printf(ses, "%-10s>%-10s {%s}",
                        ses->locations[d[i]].c_str(),
                        ses->locations[d[i-1]].c_str(),
                        r.path.c_str());
                }
            }
    }
truncated_path:
    pptr=path+(pptr!=path);
    if (*pathvar)
        set_variable(pathvar, pptr, ses);
    return ses;

not_found:
    if (!flag)
        tintin_printf(ses, "No paths from %s to %s found.", Astr, Bstr);

    return ifelse("dogoto", arg, ses);
}
