Name:       telepathy-morse

Summary:    A Telegram connection manager
Version:    0.2.0
Release:    1
Group:      Applications/Communications
License:    GPLv2+
URL:        https://github.com/TelepathyIM/telepathy-morse
Source0:    https://github.com/TelepathyIM/telepathy-morse/archive/%{name}-%{version}.tar.bz2
Requires:   telepathy-mission-control
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
Requires: qt5-qtcore
Requires: qt5-qtnetwork
Requires: openssl
Requires: libTelegramQt5 >= 0.2.0
BuildRequires: pkgconfig(dbus-1) >= 1.1.0
BuildRequires: pkgconfig(openssl)
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5Network)
BuildRequires: pkgconfig(TelegramQt5) >= 0.2.0
BuildRequires: pkgconfig(TelepathyQt5) >= 0.9.6
BuildRequires: pkgconfig(TelepathyQt5Service) >= 0.9.6
BuildRequires: pkgconfig(TelepathyQt5Farstream) >= 0.9.6
BuildRequires: cmake >= 2.8

%description
A Telegram connection manager.

# Fallback to qtc_qmake5 to qmake5, if the first one is not already defined
%{!?qtc_qmake5:%define qtc_qmake5 %qmake5}

%prep
%setup -q -n %{name}-%{version}

%build
%{qtc_qmake5} \
  "INSTALL_PREFIX=%{_prefix}" \
  "INSTALL_LIBEXEC_DIR=%{_libexecdir}" \
  "INSTALL_DATA_DIR=%{_datadir}"

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
# >> install pre
# << install pre
%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
# >> files
%{_libexecdir}/%{name}
%{_datadir}/dbus-1/services/*.service
%{_datadir}/telepathy/managers/*.manager
# << files
