README for CS 61 Problem Set 6
------------------------------
YOU MUST FILL OUT THIS FILE BEFORE SUBMITTING!
** DO NOT PUT YOUR NAME OR YOUR PARTNER'S NAME! **

RACE CONDITIONS
---------------
Write a SHORT paragraph here explaining your strategy for avoiding
race conditions. No more than 400 words please.

We used a very simple strategy centered mostly on the use of MUTEXes to avoid 
race conditions. Using a single MUTEX, we locked a thread anytime it was accessing 
the linked list in which we stored our open connections, which occurred numerous times. 
This meant that no two threads would ever access the table of connections at the same 
time, allowing us to be certain that no connection would ever be seen and desired by 
two different threads and that we wouldn't lose a connection when putting it into the 
list because of two concurrent additions. We also locked a thread whenever it received 
a `result` variable greater than 0, indicating that we should wait until that number 
of milliseconds have passed to attempt a new connection. We locked the thread, called 
`usleep(1000*result)`, then unlocked.  
While we used no new conditional variables, we did change the location of a conditional 
variable signal (pthread_cond_signal(&condvar)) in order to complete Phase 2. Doing this allows a new thread start before 
the current thread ends.



OTHER COLLABORATORS AND CITATIONS (if any):



KNOWN BUGS (if any):



NOTES FOR THE GRADER (if any):



EXTRA CREDIT ATTEMPTED (if any):
