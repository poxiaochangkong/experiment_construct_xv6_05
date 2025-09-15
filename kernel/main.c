void uartinit(void);
void uart_puts(char *s);
void printf(char *fmt, ...);
void clear_screen(void);
void clear_line(void);
int main(){
    uartinit();
    uart_puts("Hello, OS!\n");
    printf("Testinginteger:%d\n",42);
    printf("Testingnegative:%d\n",-123);
    printf("Testingzero:%d\n",0);
    printf("Testinghex:0x%x\n",0xABC);
    printf("Testingstring:%s\n","Hello");
    printf("Testingchar:%c\n",'X');
    printf("Testingpercent:%%\n");
    printf("INT_MAX:%d\n",2147483647);
    printf("INT_MIN:%d\n",-2147483648);
    printf("NULLstring:%s\n",(char*)0);
    printf("Emptystring:%s","");
    clear_screen();
    while(1);
}