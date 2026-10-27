/* VT-only native xterm. Legacy common utilities retain these entry points;
 * TekInit reports that no graphics backend exists. No parser, drawing state
 * or Tek font tables are linked, and main always runs the VT parser. */
int TekInit(void) { return 0; }
void TCursorToggle(void) {}
void TekCopy(void) {}
void TekEnqMouse(int c) { (void)c; }
void TekExpose(void) {}
void TekGINoff(void) {}
void TekReverseVideo(void) {}
void TekSetFontSize(int n) { (void)n; }
void TekSimulatePageButton(void) {}
void dorefresh(void) {}
