// stack.c

int add_func(int a, int b)
{
    int c, d;

    c = a;
    d = b;

    return c + d;
}

int main(int argc, char *argv[])
{
    int total;

    total = add_func(1, 2);

    return 0;
}
