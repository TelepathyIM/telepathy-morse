Name:       telepathy-morse
Summary:    A Telegram connection manager
Version:    0.2.0
Release:    1
Group:      Applications/Communications
License:    GPLv2+
URL:        https://github.com/TelepathyIM/telepathy-morse
Source0:    https://github.com/TelepathyIM/telepathy-morse/archive/%{name}-%{version}.tar.bz2
Requires:   telepathy-mission-control
BuildRequires: pkgconfig(dbus-1) >= 1.1.0
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5Network)
BuildRequires: pkgconfig(TelegramQt5) >= 0.2.0
BuildRequires: pkgconfig(TelepathyQt5) >= 0.9.6
BuildRequires: pkgconfig(TelepathyQt5Service) >= 0.9.6
BuildRequires: pkgconfig(TelepathyQt5Farstream) >= 0.9.6
BuildRequires: cmake >= 3.2

%description
A Telegram connection manager.

%prep
%setup -q -n %{name}-%{version}

%build
%cmake \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DCMAKE_INSTALL_LIBEXECDIR=%{_libexecdir} \
    -DCMAKE_INSTALL_DATADIR=%{_datadir} \
    .

%__make %{?_smp_mflags}

%install
%make_install

%clean
%__rm -rf "%{buildroot}"

%files
%defattr(-,root,root,-)
%{_libexecdir}/%{name}
%{_datadir}/dbus-1/services/*.service
%{_datadir}/telepathy/managers/*.manager
