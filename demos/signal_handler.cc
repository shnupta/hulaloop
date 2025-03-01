#include <hulaloop/loop.h>

int main(int argc, char** argv) {
  hula::loop loop;

  auto c = loop.schedule(5s, [&] { loop.stop(); });

  // change your terminal size!
  auto winch_handler = loop.connect_to_unix_signal(
      hula::unix::sig::sigwinch, [] { printf("got sigwinch!\n"); });

  loop.run();

  return 0;
}
