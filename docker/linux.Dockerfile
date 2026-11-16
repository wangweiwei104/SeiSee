FROM quay.io/pypa/manylinux2014_x86_64

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

ENV QT_ROOT=/home/ww/Qt/5.15.2/gcc_64
ENV PATH=${QT_ROOT}/bin:${PATH}
ENV LD_LIBRARY_PATH=${QT_ROOT}/lib
ENV QMAKE=${QT_ROOT}/bin/qmake
ENV QT_PLUGIN_PATH=${QT_ROOT}/plugins
ENV QT_QPA_PLATFORM_PLUGIN_PATH=${QT_ROOT}/plugins/platforms
ENV QML2_IMPORT_PATH=${QT_ROOT}/qml
ENV CMAKE_PREFIX_PATH=${QT_ROOT}
ENV PKG_CONFIG_PATH=${QT_ROOT}/lib/pkgconfig
ENV LINUXDEPLOY=/usr/local/bin/linuxdeploy
ENV APPIMAGETOOL=/usr/local/bin/appimagetool
ENV SEISEE_BUILD_DIR=/workspace/build/docker-release
ENV HOME=/tmp

RUN yum --disablerepo='*' \
        --enablerepo=base \
        --setopt=base.baseurl=https://mirrors.tuna.tsinghua.edu.cn/centos-vault/7.9.2009/os/x86_64/ \
        install -y \
        curl \
        fontconfig \
        libX11 \
        libXext \
        libXrender \
        libxcb \
        libxkbcommon \
        libxkbcommon-x11 \
        mesa-libGL \
        xcb-util \
        xcb-util-image \
        xcb-util-keysyms \
        xcb-util-renderutil \
        xcb-util-wm && \
    yum clean all && \
    rm -rf /var/cache/yum

RUN curl -fsSL \
        https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
        -o "${LINUXDEPLOY}" && \
    curl -fsSL \
        https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage \
        -o /usr/local/bin/linuxdeploy-plugin-qt && \
    curl -fsSL \
        https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage \
        -o "${APPIMAGETOOL}" && \
    chmod 755 "${LINUXDEPLOY}" /usr/local/bin/linuxdeploy-plugin-qt \
        "${APPIMAGETOOL}"

WORKDIR /workspace
COPY . /workspace

# Qt is bind-mounted at QT_ROOT when running the container; builds use the host UID.
RUN chmod -R a+rwX /workspace

ENTRYPOINT ["/bin/bash", "/workspace/scripts/package-linux.sh"]
