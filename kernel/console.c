#define BACKSPACE 0x100
void uart_putc(char c);

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