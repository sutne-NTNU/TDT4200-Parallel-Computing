# Running on SSH with CPU and GPU:

## Part 1 - Saving files to the catalog

1. Right click Finder and select either:
   - `connect to server...`  
   - `koble til tjener...`

2. Connect to `smb://sambaad.stud.ntnu.no/<username>`

3. When prompted enter correct username and password (Feide)

4. The host will apear as a drive in finder

5. Copy program files to desired location on the server


## Part 2 - Running the code
1. Open terminal

2. Connecting to the host:
   - on NTNU-network/VPN: `ssh <username>@snotra.idi.ntnu.no`
   - otherwise: `ssh -J <username>@login.stud.ntnu.no <username>@snotra.idi.ntnu.no`

   when connecting you might be warned about unverified host, if so just type "yes"

3. Enter password when prompted (Feide)

4. When connection is established type `connect`.

5. You should now be able to use `ls` and `cd` to navigate through the files you saved in Part 1

### Stopping a running instance:

To stop a running job in a closed terminal use: `ssh <username>@snotra.idi.ntnu.no stop-jobs -f`.
Otherwise you can just use the command `exit` within the job.
