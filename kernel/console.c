#define BACKSPACE 0x100
void uart_putc(char c);
void printf(char *fmt, ...);
void
consputc(int c)
{
  if(c == BACKSPACE){
    // if the user typed backspace, overwrite with a space.
    uart_putc('\b'); uart_putc(' '); uart_putc('\b');
  } else {
    uart_putc(c);
  }
}
void 
clear_screen(void)
{
  printf("\033[2J"); // ANSI escape code: clear screen
  printf("\033[H");  // ANSI escape code: move cursor to home position
}
void 
clear_line(void)
{
  printf("\033[K"); // ANSI escape code: clear line from cursor right
}