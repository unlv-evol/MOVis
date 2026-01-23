# Development Container for MOVis

This Docker compose starts a Linux container including a desktop accessible through a web browser. While you can run the docker setup in any machine, we still recommended that you use a Linux Os with Debain/Ubuntu distribution.

All dependencies for building and running the MOVis tool are included in the container.
You must have installed [Docker](https://docs.docker.com/get-started/get-docker/) and [Git](https://git-scm.com/downloads) on your local system.

## Build the Docker Image

First clone the MOVis repository. 
```
cd <path to your git folder>
git clone  https://github.com/unlv-evol/MOVis.git
```

Assuming Docker is running, initially, build the Docker image with:
```
cd MOVis/docker/dev-container-MOVis
docker compose build --no-cache
```

Then start the container with:
```
docker compose up --build
```
In principle, you can install packages (```sudo apt update && sudo apt install -y <packages>```, no sudo password) and make changes within the containerized desktop as on a normal Ubuntu Linux OS.
However, only the user data in ```/config``` (the user's home directory) is mapped to a Docker volume ```dev_container_MOVis_data``` (see docker-compose.yml), i.e., if you delete the container, only changes wrt. the volume are persistent until you also delete the volume.
 (Checkout the corresponding pages in the Docker Desktop UI or use ```docker ps``` and ```docker volume ls``` to list your containers and volumes.)

## Access the Remote Desktop

### KasmVNC:

After the container has been started, go to ```http://localhost:3000/``` in your browser.
The desktop is only accessible locally.
Note, to use it remotely you would have to remove ```127.0.0.1``` from the docker-compose.yml and consider using a password and HTTPS ```https://localhost:3001/```.
Adjusting the size of the browser window will set the according screen resolution of the desktop.
In the browser window in the left (initially collapsed) side panel, you can also edit settings such as the streaming quality.

#### ```Windowa Issue:``` Clock Synchronization

TLDR; If you use *Windows Docker with WSL* make sure your clock is correctly, recently synchronized. 
Otherwise, KasmVNC can cause memory spikes that may crash the desktop session.
Run "synchronize now"  in the settings app (Time and Language > Date and Time) or run```w32tm /resync``` in terminal (administrator).
You can also automate this using the script below.
Lowering the streaming quality (medium streaming setting at FHD 1920 × 1080 pixel, 24 FPS) can also help. 
Alternatively, use RustDesk as described below.

1. Create a new file: C:\Scripts\sync-clock.vbs
   ```
   Set objShell = CreateObject("WScript.Shell")
   objShell.Run "powershell.exe -ExecutionPolicy Bypass -Command ""net start w32time; w32tm /resync""", 0, False
   ```
1. Windows + R: taskschd.msc
1. Create Task... (not Create Basic Task...)
1. General -> Name: Sync-Clock
1. Select Run with highest privileges.
1. Triggers -> New...
   - On workstation unlock
1. Triggers -> New...  
   - On a schedule (One time) -> Advanced settings: Repeat every 1 hour for a duration of Indefinitely
1. Action -> New...
   - Program: wscript.exe
   - Arguments: C:\Scripts\sync-clock.vbs

## Installing and Running MOVis in the Dev-Container
This section will get you through installation and execution of the MOVis tool using docker setup. 

Starting the container would be comparable to booting the OS.
```
docker start dev-container-MOVis
```

The corresponding project is in the config folder in the home directory: ```/config/MOVis```

Stopping the container would be comparable to a shut down of the OS.
This is also what Docker does if you shut down your host system or Docker itself; therefore, you may have to start the container after rebooting.
```
docker stop dev-container-MOVis
```
