Name:		calendar
Version:	0.1.0
Release:	1%{?dist}
Summary:	UNIX calendar(1) reminder service with Chinese calendar support

License:	BSD
URL:		https://github.com/liweitianux/ccalendar

BuildRequires:	make, gcc

%description
The "calendar" utility provides a reminder service by checking a calendar file
and showing events within the specified date range.  This "calendar" utility
has Chinese calendar support implemented with astronomical algorithms.

%prep
# empty

%build
export CFLAGS="$RPM_OPT_FLAGS"
make PREFIX=%{_prefix} MAN_DIR=%{_mandir} CALENDAR_DIR=%{_datadir}/%{name}

%install
rm -rf $RPM_BUILD_ROOT
make install PREFIX=%{buildroot}%{_prefix} \
	MAN_DIR=%{buildroot}%{_mandir} \
	CALENDAR_DIR=%{buildroot}%{_datadir}/%{name}

%files
%{_bindir}/%{name}
%{_mandir}/man1/%{name}.1.gz
%{_datadir}/%{name}/*
%doc README.md
%license LICENSE

%changelog
* Sat May 09 2020 Aaron LI <aly@aaronly.me> - 0.1.0-1
- Initial package for version 0.1.0
