#include <string>
#include <vector>


using std::string, std::vector;

vector<int> arean_as_path; //arena, stores paths

class Announcement {
	public:
		//all moved to 8 bypte boundary, ie padded added under the hood. go from largets to smallest member variable
		//for least size
		//
		//
	
		//index the object
		uint32_t prefix_id;


		// 24 bytes
		// std::string prefix; // 1.2.0.0/16 or some ipv6 prefix
		
		// 24 bytes
		// vector<uint32_t> as_path; //regular int not work, signed int at begining (?)
		
		uint32_t start_idx;
		uint32_t end_idx;

		// 4 bytes
		uint32_t next_hop_asn;
		
		// 2 bits padded to 1 byte
                short relationship; // 0 for customer, 1 for peer, 2 for provider, 3 for origin
		
};
