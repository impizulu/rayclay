/*
    orders.h - the dataset seam: swap this file, keep the whole app

    Everything above reads an array of Order and knows nothing about where the
    rows came from. Replace orders_init with a CSV reader or a SQLite query and
    not one line of the UI changes; a slow one runs in .updateCallback once the
    window has drawn, never before rcRunApp (getting-started.md, ex27).

    The rows are two years of sales at Ridgeline Outfitters, an invented outdoor
    gear shop. They are generated, not shipped, so a browser tab needs no file
    and one seed gives every platform the same 12,000 orders: the same picture
    everywhere is something a screenshot can check. Money is whole cents in an
    integer, so a column of orders adds up to its total exactly.

    A row keeps one small index per dimension in dim[], so a filter, a facet
    count and a group-by are each one loop over dim[], never a switch per column.
    Every name table is in alphabetical order, so sorting a column by its index
    is sorting it by name. Include rayclay.h first.
*/
#ifndef APP_ORDERS_H
#define APP_ORDERS_H

enum {
    ORDER_COUNT = 12000,
    ORDER_DAYS  = 730,     /* day 0 is Nov 1, 2024; the last is Oct 31, 2026 */
    ORDER_FIRST_ID = 100001,
    PRODUCT_COUNT = 32,
    FIRST_NAMES = 40,
    LAST_NAMES  = 40,
    CHANNEL_RETAIL = 3     /* into CHANNEL_NAME: a shop hands the goods over at the register */
};

/* The dimensions, in the order the facet rail lists them. */
typedef enum Dim {
    DIM_CATEGORY = 0,
    DIM_REGION,
    DIM_CHANNEL,
    DIM_STATUS,
    DIM_PAYMENT,
    DIM_SEGMENT,
    DIM_PRODUCT,
    DIM_COUNT
} Dim;

enum { DIM_MAX = PRODUCT_COUNT };   /* the widest dimension: a filter is 32 bits */

typedef enum Status {
    STATUS_DELIVERED = 0,
    STATUS_PROCESSING,
    STATUS_REFUNDED,
    STATUS_SHIPPED,
    STATUS_COUNT
} Status;

typedef struct Order {
    int32_t  cents;          /* what the customer paid, after the discount       */
    uint16_t day;            /* 0 .. ORDER_DAYS - 1; the array is in day order   */
    uint8_t  first, last;    /* indices into FIRST_NAME and LAST_NAME            */
    uint8_t  dim[DIM_COUNT];
    uint8_t  units;
    uint8_t  discount;       /* percent                                          */
} Order;

typedef struct OrderBook {
    Order    row[ORDER_COUNT];
    uint16_t monthOf[ORDER_DAYS];   /* months since Nov 2024, by day */
} OrderBook;

typedef struct Product {
    const char *name;
    uint8_t     category;
    uint8_t     weight;      /* how often it sells, relative to the others */
    int32_t     cents;
} Product;

static const char *const DIM_NAME[DIM_COUNT] = {
    "Category", "Region", "Channel", "Status", "Payment", "Segment", "Product"
};

/* What a typed filter calls each dimension: region:west. */
static const char *const DIM_KEY[DIM_COUNT] = {
    "category", "region", "channel", "status", "payment", "segment", "product"
};

static const char *const CATEGORY_NAME[6] = {
    "Accessories", "Apparel", "Camp", "Climbing", "Footwear", "Packs"
};
static const char *const REGION_NAME[6] = {
    "Canada", "Midwest", "Northeast", "Southeast", "Southwest", "West"
};
static const char *const CHANNEL_NAME[5] = {
    "Android app", "iOS app", "Marketplace", "Retail store", "Web"
};
static const char *const STATUS_NAME[STATUS_COUNT] = {
    "Delivered", "Processing", "Refunded", "Shipped"
};
static const char *const PAYMENT_NAME[5] = {
    "Apple Pay", "Gift card", "Mastercard", "PayPal", "Visa"
};
static const char *const SEGMENT_NAME[3] = { "Member", "New", "Returning" };

static const Product PRODUCT[PRODUCT_COUNT] = {
    { "Alpine GTX Boot",         4,  6, 24900 },
    { "Apex Dynamic Rope 60m",   3,  3, 18900 },
    { "Approach Pro Shoe",       4,  5, 15200 },
    { "Ascent 45L Backpack",     5,  6, 21500 },
    { "Basecamp Slip-On",        4,  8,  8800 },
    { "Cloud Sleeping Pad",      2,  6, 12900 },
    { "Creekside Sandal",        4,  8,  7400 },
    { "Crux Climbing Shoe",      3,  3, 13900 },
    { "Daybreak 22L Daypack",    5, 10,  9800 },
    { "Drift Down Jacket",       1,  7, 22900 },
    { "Ember 20F Sleeping Bag",  2,  5, 21900 },
    { "Hip Pack 4L",             5, 11,  4200 },
    { "Insulated Bottle 1L",     0, 15,  3600 },
    { "Lumen 400 Headlamp",      2, 11,  4400 },
    { "Merino Base Layer",       1, 11,  8900 },
    { "Merino Hiking Sock",      0, 20,  2200 },
    { "Polar Beanie",            0, 12,  3200 },
    { "Ridge Hiking Pant",       1, 10,  9400 },
    { "Ridge Runner Trail Shoe", 4, 10, 13800 },
    { "Stormguard 2P Tent",      2,  4, 34900 },
    { "Stormline Rain Shell",    1,  8, 17900 },
    { "Summit Mid Hiking Boot",  4,  8, 18900 },
    { "Summit Roll-Top 30L",     5,  5, 14800 },
    { "Sun Hoodie",              1, 12,  6200 },
    { "Talon Carabiner Set",     3,  5,  6400 },
    { "Titan Camp Chair",        2,  8,  7900 },
    { "Trail Cap",               0, 14,  2800 },
    { "Trail Stove Kit",         2,  7,  6900 },
    { "Trailhead Fleece",        1, 10, 10900 },
    { "Transit Duffel 60L",      5,  6, 12900 },
    { "Trek Pole Pair",          0,  6, 11900 },
    { "Vector Climbing Harness", 3,  4,  7900 }
};

static const char *const FIRST_NAME[FIRST_NAMES] = {
    "Aaliyah", "Adrian", "Amara", "Andre", "Bianca", "Caleb", "Camila", "Daniel",
    "Diego", "Elena", "Emeka", "Farah", "Felix", "Grace", "Hannah", "Hugo",
    "Imani", "Isaac", "Jonah", "Julia", "Kai", "Leah", "Liam", "Lucia",
    "Marcus", "Maya", "Mei", "Nadia", "Noah", "Olivia", "Owen", "Priya",
    "Quinn", "Rafael", "Rosa", "Samir", "Sofia", "Tessa", "Theo", "Zoe"
};

static const char *const LAST_NAME[LAST_NAMES] = {
    "Adeyemi", "Almeida", "Bauer", "Bennett", "Brooks", "Castillo", "Chen", "Cohen",
    "Delgado", "Dubois", "Ellis", "Fischer", "Garcia", "Hansen", "Hayes", "Ito",
    "Jensen", "Kapoor", "Kim", "Larsen", "Lewis", "Mendez", "Morgan", "Murphy",
    "Nakamura", "Novak", "Okafor", "Olsen", "Park", "Patel", "Reyes", "Rossi",
    "Santos", "Schmidt", "Singh", "Sullivan", "Tran", "Walsh", "Weber", "Young"
};

static const char *const MONTH_NAME[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/** The name of value @p v of dimension @p d: the one lookup every label uses. */
static inline const char *dim_value(int d, int v) {
    switch (d) {
    case DIM_CATEGORY: return CATEGORY_NAME[v];
    case DIM_REGION:   return REGION_NAME[v];
    case DIM_CHANNEL:  return CHANNEL_NAME[v];
    case DIM_STATUS:   return STATUS_NAME[v];
    case DIM_PAYMENT:  return PAYMENT_NAME[v];
    case DIM_SEGMENT:  return SEGMENT_NAME[v];
    default:           return PRODUCT[v].name;
    }
}

static inline int dim_size(int d) {
    static const int size[DIM_COUNT] = { 6, 6, 5, STATUS_COUNT, 5, 3, PRODUCT_COUNT };

    return size[d];
}

/* An order's later dates are derived from its row, so the data needs no more columns.
   These are the most days after the order each step can take: orders_init gives a status
   only to an order old enough to have reached it by the last day of data. */
enum { SHIP_DAYS_MAX = 2, DELIVER_DAYS_MAX = 7, REFUND_DAYS_MAX = 16 };

static inline int order_shipped_day(const Order *o, int row) {
    return o->day + 1 + row % 2;
}

static inline int order_delivered_day(const Order *o, int row) {
    return order_shipped_day(o, row) + 3 + row % 3;
}

static inline int order_refunded_day(const Order *o, int row) {
    int handed = o->dim[DIM_CHANNEL] == CHANNEL_RETAIL ? o->day : order_delivered_day(o, row);

    return handed + 4 + row % 6;
}

/** Calendar date of a day index. Months are walked, not computed: the shop's two
    years hold one leap-free February each, so a table is the whole calendar. */
static inline void day_to_date(int day, int *year, int *month, int *date) {
    static const uint8_t length[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int y = 2024, m = 10;

    while (day >= length[m]) {
        day -= length[m];
        if (++m == 12) {
            m = 0;
            y++;
        }
    }
    *year  = y;
    *month = m;
    *date  = day + 1;
}

/** xorshift32 (Marsaglia 2003). The requirement is reproducibility, not statistical
    quality: 32-bit shifts and xors are exactly defined, so every target draws the
    same orders. */
static inline uint32_t order_rnd(uint32_t *s) {
    uint32_t x = *s;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

/** Pick an index from @p n integer weights, proportionally. */
static inline int order_pick(uint32_t *s, const uint32_t *weight, int n) {
    uint32_t total = 0, r;
    int      i;

    for (i = 0; i < n; i++)
        total += weight[i];
    r = order_rnd(s) % total;
    for (i = 0; i < n - 1; i++) {
        if (r < weight[i])
            return i;
        r -= weight[i];
    }
    return n - 1;
}

/** Fill @p b with the same 12,000 orders every time.

    A day's order count follows the season, the weekday and two years of growth.
    Each order then picks a product by the season (tents sell in summer, jackets
    at the holidays) and a channel by the year (the apps gain on the web), so the
    charts have real shapes to show once they are split and filtered. */
static inline void orders_init(OrderBook *b, uint32_t seed) {
    static const uint8_t MONTH_DEMAND[12] = {
        72, 62, 78, 96, 112, 118, 104, 92, 84, 88, 134, 158
    };
    static const uint8_t WEEKDAY_DEMAND[7] = { 92, 90, 94, 98, 104, 118, 110 };
    /* Per category, the months it runs hot in, one bit each with January lowest. */
    static const uint16_t SEASON[6] = { 0x0C01, 0x0C03, 0x01F0, 0x0270, 0x0078, 0x0E30 };
    static const uint8_t UNITS[12]    = { 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 3, 4 };
    static const uint8_t DISCOUNT[10] = { 0, 0, 0, 0, 0, 0, 5, 10, 15, 20 };
    static const uint32_t REGION_MIX[6]  = { 8, 18, 22, 17, 12, 23 };
    static const uint32_t SEGMENT_MIX[3] = { 18, 38, 44 };
    /* Refunds per thousand, by channel: a marketplace returns most, a shop least. */
    static const uint16_t REFUND_RATE[5] = { 62, 58, 118, 26, 74 };
    uint32_t s = seed ? seed : 1u;
    uint32_t dayWeight[ORDER_DAYS];
    uint64_t sum = 0, soFar = 0;
    int      made = 0, d, i;

    for (d = 0; d < ORDER_DAYS; d++) {
        int y, m, date;

        day_to_date(d, &y, &m, &date);
        b->monthOf[d] = (uint16_t)((y - 2024) * 12 + m - 10);
        dayWeight[d] = (uint32_t)(MONTH_DEMAND[m] * WEEKDAY_DEMAND[(d + 4) % 7] *
                                  (500 + d / 2) * (85 + order_rnd(&s) % 31) / 1000);
        sum += dayWeight[d];
    }

    for (d = 0; d < ORDER_DAYS; d++) {
        int m = (10 + b->monthOf[d]) % 12;
        int target;

        soFar += dayWeight[d];
        target = (int)(soFar * ORDER_COUNT / sum);

        for (; made < target; made++) {
            Order   *o = &b->row[made];
            uint32_t weight[PRODUCT_COUNT], mix[5];
            int      product, channel, off, age = ORDER_DAYS - 1 - d;
            int64_t  gross;

            for (i = 0; i < PRODUCT_COUNT; i++) {
                bool hot = (SEASON[PRODUCT[i].category] >> m) & 1;

                weight[i] = PRODUCT[i].weight * (hot ? 3u : 2u);
            }
            product = order_pick(&s, weight, PRODUCT_COUNT);

            /* Android, iOS, marketplace, retail, web: both apps grow over the years. */
            mix[0] = (uint32_t)(10 + d / 60);
            mix[1] = (uint32_t)(14 + d / 45);
            mix[2] = 16;
            mix[3] = 14;
            mix[4] = (uint32_t)(46 - d / 30);
            channel = order_pick(&s, mix, 5);

            o->day    = (uint16_t)d;
            o->first  = (uint8_t)(order_rnd(&s) % FIRST_NAMES);
            o->last   = (uint8_t)(order_rnd(&s) % LAST_NAMES);
            o->units  = UNITS[order_rnd(&s) % 12];
            /* Discounts come more often in November and December. */
            off = (int)(order_rnd(&s) % 10);
            if (m >= 10 && off < 3)
                off = 9 - off;
            o->discount = DISCOUNT[off];
            o->dim[DIM_CATEGORY] = PRODUCT[product].category;
            o->dim[DIM_PRODUCT]  = (uint8_t)product;
            o->dim[DIM_CHANNEL]  = (uint8_t)channel;
            o->dim[DIM_REGION]   = (uint8_t)order_pick(&s, REGION_MIX, 6);
            o->dim[DIM_SEGMENT]  = (uint8_t)order_pick(&s, SEGMENT_MIX, 3);

            /* A retail store takes cards, the iOS app mostly Apple Pay. */
            if (channel == CHANNEL_RETAIL)
                o->dim[DIM_PAYMENT] = (uint8_t)(order_rnd(&s) % 2 ? 2 : 4);
            else if (channel == 1 && order_rnd(&s) % 10 < 6)
                o->dim[DIM_PAYMENT] = 0;
            else if (order_rnd(&s) % 20 == 0)
                o->dim[DIM_PAYMENT] = 1;
            else
                o->dim[DIM_PAYMENT] = (uint8_t)(2 + order_rnd(&s) % 3);

            /* The newest orders are still moving through the warehouse, unless they were
               carried out of a shop. */
            if (age < SHIP_DAYS_MAX && channel != CHANNEL_RETAIL)
                o->dim[DIM_STATUS] = STATUS_PROCESSING;
            else if (age < DELIVER_DAYS_MAX && channel != CHANNEL_RETAIL)
                o->dim[DIM_STATUS] = STATUS_SHIPPED;
            else if (age >= REFUND_DAYS_MAX && order_rnd(&s) % 1000 < REFUND_RATE[channel])
                o->dim[DIM_STATUS] = STATUS_REFUNDED;
            else
                o->dim[DIM_STATUS] = STATUS_DELIVERED;

            gross    = (int64_t)PRODUCT[product].cents * o->units;
            o->cents = (int32_t)(gross * (100 - o->discount) / 100);
        }
    }
}

#endif /* APP_ORDERS_H */
