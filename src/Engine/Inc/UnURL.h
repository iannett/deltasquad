#pragma once

// how a new url takes over parts of the one it travels from
enum ETravelType
{
    TRAVEL_Absolute = 0, // a new url with only default options added
    TRAVEL_Partial  = 1, // keeps the base url's options
    TRAVEL_Relative = 2, // keeps the base url's protocol, host, map, portal, port and options
};
