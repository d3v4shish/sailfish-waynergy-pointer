Name:           sailfish-deskflow-setup
Version:        0.1.0
Release:        1
Summary:        Guided Deskflow, Waynergy, and pointer setup for Sailfish OS
License:        MIT
Group:          Applications/System
BuildArch:      armv7hl
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  qt5-qtdeclarative-devel

%description
Developer-mode installer for the custom Waynergy client, a Debian Bookworm
chroot, and the Sailfish Lipstick pointer indicator.

%prep
%setup -q

%build
pushd installer-app
qmake sailfish-deskflow-setup.pro
make %{?_smp_mflags}
popd
pushd pointer-plugin
qmake waynergypointer.pro
make %{?_smp_mflags}
popd

%install
install -d %{buildroot}/usr/bin
install -m 0755 installer-app/sailfish-deskflow-setup %{buildroot}/usr/bin/sailfish-deskflow-setup
install -d %{buildroot}/usr/share/applications
install -m 0644 installer-app/sailfish-deskflow-setup.desktop %{buildroot}/usr/share/applications/
install -m 0644 calibrator/waynergy-pointer-calibrator.desktop %{buildroot}/usr/share/applications/
install -d %{buildroot}/usr/share/sailfish-deskflow-setup
cp -a waynergy lipstick %{buildroot}/usr/share/sailfish-deskflow-setup/
install -m 0644 installer/rootfs-manifest.ini %{buildroot}/usr/share/sailfish-deskflow-setup/
install -m 0644 calibrator/Calibrator.qml %{buildroot}/usr/share/sailfish-deskflow-setup/
install -d %{buildroot}/usr/libexec/sailfish-deskflow-setup
install -m 0755 installer/root-helper %{buildroot}/usr/libexec/sailfish-deskflow-setup/root-helper
install -d %{buildroot}/usr/lib/qt5/qml/Waynergy/Pointer
install -m 0644 pointer-plugin/libwaynergypointer.so %{buildroot}/usr/lib/qt5/qml/Waynergy/Pointer/
install -m 0644 pointer-plugin/PointerOverlay.qml pointer-plugin/qmldir %{buildroot}/usr/lib/qt5/qml/Waynergy/Pointer/

%files
%license LICENSE
/usr/bin/sailfish-deskflow-setup
/usr/share/applications/sailfish-deskflow-setup.desktop
/usr/share/applications/waynergy-pointer-calibrator.desktop
/usr/share/sailfish-deskflow-setup
/usr/libexec/sailfish-deskflow-setup
/usr/lib/qt5/qml/Waynergy/Pointer

%changelog
* Mon Sep 08 2026 Sailfish Waynergy Pointer contributors <noreply@example.invalid> - 0.1.0-1
- Initial developer-mode installer
