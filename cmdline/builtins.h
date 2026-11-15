typedef struct MoolahCliContext MoolahCliContext;

static void help(MoolahCliContext *ctx)
{
    printf("%s\n", ctx->parser->config->usage);
    exit(0);
}

static void fuckyoutoo(MoolahCliContext *ctx)
{
    printf("fuck you too\n");
    exit(0);
}

static void yomomma(MoolahCliContext *ctx)
{
    printf("yo momma so poor she had to drink coffee grounds to get high\n");
    exit(0);
}

static void yousuck(MoolahCliContext *ctx)
{
    printf("you suck\n");
    exit(0);
}

static void skillissue(MoolahCliContext *ctx)
{
    printf("fatal: skill issue\n");
    exit(1);
}

static void getgood(MoolahCliContext *ctx)
{
    printf("get good. immediately.\n");
    exit(0);
}

static void what(MoolahCliContext *ctx)
{
    printf("what the fuck did you expect\n");
    exit(0);
}

static void bonk(MoolahCliContext *ctx)
{
    printf("bonk\n");
    exit(0);
}

static void perish(MoolahCliContext *ctx)
{
    printf("perish.\n");
    exit(0);
}

static void kys(MoolahCliContext *ctx)
{
    printf("no\n");
    exit(0);
}

static void bruh(MoolahCliContext *ctx)
{
    printf("bruh\n");
    exit(0);
}

static void skillissuecheck(MoolahCliContext *ctx)
{
    printf("skill issue confirmed\n");
    exit(1);
}

static void uninstall(MoolahCliContext *ctx)
{
    printf("you have been uninstalled from the application\n");
    exit(0);
}

static void segfault(MoolahCliContext *ctx)
{
    printf("segmentation fault (core emotionally dumped)\n");
    exit(1);
}

static void touchgrass(MoolahCliContext *ctx)
{
    printf("ERROR: grass not found\n");
    printf("Please install grass and try again.\n");
    exit(1);
}

static void imagine(MoolahCliContext *ctx)
{
    printf("imagine needing help for this\n");
    exit(0);
}

static void cope(MoolahCliContext *ctx)
{
    printf("cope harder\n");
    exit(0);
}

static void seethe(MoolahCliContext *ctx)
{
    printf("seethe detected\n");
    exit(0);
}

static void mald(MoolahCliContext *ctx)
{
    printf("malding...\n");
    exit(0);
}

static void skillissueagain(MoolahCliContext *ctx)
{
    printf("we already discussed this. skill issue.\n");
    exit(1);
}

static void whatcouldgowrong(MoolahCliContext *ctx)
{
    printf("everything.\n");
    exit(0);
}

static void developer(MoolahCliContext *ctx)
{
    printf("it works on my machine\n");
    exit(0);
}

static void worksforme(MoolahCliContext *ctx)
{
    printf("works for me ¯\\_(ツ)_/¯\n");
    exit(0);
}

static void sudo_rm_rf(MoolahCliContext *ctx)
{
    printf("absolutely fucking not\n");
    exit(1);
}

static void amogus(MoolahCliContext *ctx)
{
    printf("ඞ\n");
    exit(0);
}

static void ohio(MoolahCliContext *ctx)
{
    printf("the CLI has entered an unstable geopolitical state\n");
    exit(0);
}

static void nuclear(MoolahCliContext *ctx)
{
    printf("Launching absolutely nothing...\n");
    printf("Nothing launched successfully.\n");
    exit(0);
}

static void fuckit(MoolahCliContext *ctx)
{
    printf("fuck it, we ball\n");
    exit(0);
}

static void ball(MoolahCliContext *ctx)
{
    printf("we ball\n");
    exit(0);
}

static void rickroll(MoolahCliContext *ctx)
{
    printf("You know the rules, and so do I.\n");
    exit(0);
}


static void adlib(MoolahCliContext *ctx)
{
    printf("Young moola baby...a milli a milli...\n");
    exit(0);
}

static void whatsgood(MoolahCliContext *ctx)
{
    printf("nothing just chillin in your fuckass %s program\n", ctx->parser->config->program);
    exit(0);
}