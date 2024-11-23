#pragma once

#include <string>


namespace netutils {

	bool IsValidIPAddress(std::string_view ipAddress);

	// Platform-specific implementations
	std::string ResolveDomainName(std::string_view name);

}
