# https://docs.linuxserver.io/images/docker-webtop/
# https://github.com/linuxserver/docker-webtop
# https://github.com/linuxserver/docker-webtop/releases
# FIXME: Memory crashes on WSL: dmesg -T | grep -i oom
FROM lscr.io/linuxserver/webtop:ubuntu-kde-951db35e-ls75

##### PORTS #####

# HTTP VNC
EXPOSE 3000
# HTTPS VNC
EXPOSE 3001

##### ENVIRONMENT VARIABLES #####

# User and Group ID
ENV PUID=1000
ENV PGID=1000

ENV HOME=/config

##### INSTALL PACKAGES #####

# 1) System deps + add repo for Python 3.12 (deadsnakes) + install tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    software-properties-common \
    ca-certificates \
    curl \
    gnupg \
    wget \
    unzip \
    git \
    nano \
    htop \
    dos2unix \
    npm \
    libgl1-mesa-dev \
    mesa-common-dev \
    && add-apt-repository -y ppa:deadsnakes/ppa \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
    python3.12 \
    python3.12-venv \
    python3.12-dev \
    python3-pip \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# 2) Make Python 3.12 the default "python" and "python3"
RUN update-alternatives --install /usr/bin/python3 python3 /usr/bin/python3.12 312 \
    && update-alternatives --install /usr/bin/python  python  /usr/bin/python3.12 312

# 3) Install pip for Python 3.12 (reliable across Ubuntu variants)
# RUN curl -sS https://bootstrap.pypa.io/get-pip.py | python3.12

# (Optional but recommended) verify
RUN python3 --version && python --version && pip --version


# RUN java -XshowSettings:properties -version
# # Find the actual JAVA_HOME path dynamically
# RUN echo "JAVA_HOME should be: $(dirname $(dirname $(readlink -f $(which java))))"

# # Java
# RUN set -eux; \
#     JAVA_HOME_REAL="$(dirname "$(dirname "$(readlink -f "$(which javac)")")")"; \
#     echo "Detected JAVA_HOME: $JAVA_HOME_REAL"; \
#     ln -sfn "$JAVA_HOME_REAL" /usr/lib/jvm/default-java; \
#     echo "export JAVA_HOME=/usr/lib/jvm/default-java" > /etc/profile.d/java_home.sh

# ENV JAVA_HOME=/usr/lib/jvm/default-java
# ENV PATH="${JAVA_HOME}/bin:${PATH}"

# ENV JAVA_HOME=/usr/lib/jvm/java-11-openjdk-arm64
# ENV PATH=$JAVA_HOME/bin:$PATH
	
# Prepare Git folder:
# RUN mkdir -p $HOME/git

# ##### MOVis #####

# # Clone repository: MOvis
# RUN git clone https://github.com/unlv-evol/MOVis.git $HOME/git/MOVis

# # Setup Git identity for MOvis
# RUN git config --global user.email "you@example.com"
# RUN git config --global user.name "Your Name"
# RUN git config --global init.defaultBranch main



##### CREATE DESKTOP SHORTCUTS #####

# Clear default desktop shortcuts
RUN rm -rf /home/kasm-default-profile/Desktop

# Ensure the Desktop directory exists
RUN mkdir -p $HOME/Desktop
RUN mkdir -p $HOME/MOVis

# Download and extract MOVis release
RUN wget -O /tmp/MOVis-Linux.zip \
    https://raw.githubusercontent.com/unlv-evol/MOVis/main/release/MOVis-Linux.zip && \
    unzip /tmp/MOVis-Linux.zip -d $HOME/MOVis/ && \
    rm /tmp/MOVis-Linux.zip

RUN echo "Top-level contents of /config/MOVis:" && ls -lah $HOME/MOVis/

# Install Python dependencies
RUN pip install --no-cache-dir --ignore-installed -r $HOME/MOVis/Desktop_Qt_6_8_3-Release/Pareco/requirements.txt --break-system-packages

RUN mkdir -p $HOME/.config/my-scripts
RUN mkdir -p $HOME/.config/autostart

# Add resolution scripts	
COPY Dockerfiles/add_resolution.sh $HOME/.config/my-scripts/add_resolution.sh
COPY Dockerfiles/add_resolutions.sh $HOME/.config/my-scripts/add_resolutions.sh
RUN chmod +x $HOME/.config/my-scripts/add_resolution.sh && \
    chmod +x $HOME/.config/my-scripts/add_resolutions.sh && \
    dos2unix $HOME/.config/my-scripts/add_resolution.sh && \
	dos2unix $HOME/.config/my-scripts/add_resolutions.sh
RUN printf "[Desktop Entry]\n\
Type=Application\n\
Name=Resolution\n\
Exec=bash $HOME/.config/my-scripts/add_resolutions.sh\n\
StartupNotify=false\n\
X-GNOME-Autostart-enabled=true\n" > $HOME/.config/autostart/resolutions.desktop && \
    chmod +x $HOME/.config/autostart/resolutions.desktop

##### MOVE: /home/kasm-user > /config #####

# Workaround: during Docker build, e.g., Maven, uses /home/kasm-user but our abc user home directory is /config
RUN mv /home/kasm-user/* /home/kasm-user/.* $HOME/ 2>/dev/null || true

##### SET USER FOLDER PERMISSIONS #####

USER root
RUN chown -R $PUID:$PGID $HOME
USER $PUID
RUN chmod -R u+rw $HOME
USER root