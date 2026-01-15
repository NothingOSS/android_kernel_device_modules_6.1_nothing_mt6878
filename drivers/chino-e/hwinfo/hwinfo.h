#ifndef KEYWORD

#define KEYWORD_ENUM
#define KEYWORD(symbol) symbol,

enum HWINFO_E{
#endif


KEYWORD(board_id)
KEYWORD(serialno)
KEYWORD(hw_sku)
KEYWORD(battery_charging_enabled)    //battary  capacity
KEYWORD(battery_input_suspend)    //battary  capacity


#ifdef KEYWORD_ENUM
KEYWORD(HWINFO_MAX)
};
int smartisan_hwinfo_register(enum HWINFO_E e_hwinfo,char *hwinfo_name);
#undef KEYWORD_ENUM
#undef KEYWORD

#endif
