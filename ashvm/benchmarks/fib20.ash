forge fib(n) { given (n < 2) { yield n; } yield fib(n-1)+fib(n-2); } say fib(20);
