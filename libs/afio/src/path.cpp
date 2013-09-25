#include "../../../boost/afio/path.hpp"
#include "../../../boost/afio/directory_monitor_v2.hpp"


size_t poll_rate = 50;
namespace boost{
	namespace afio{
		std::pair< future< bool >, async_io_op > Path::remove_ent(std::unordered_map<directory_entry, directory_entry>& dict, const async_io_op & req, const directory_entry& ent)
		{
			auto func = [this, &ent, &dict]() -> bool {
				return this->remove_ent(dict, ent);
			};
			
			return dispatcher->call(req, func);
		}

		std::pair< future< bool >, async_io_op > Path::add_ent(std::unordered_map<directory_entry, directory_entry>& dict, const async_io_op & req, const directory_entry& ent)
		{
			auto func = [this, &ent, &dict]() -> bool {
				return this->add_ent(dict, ent);
			};
			return dispatcher->call(req, func);
		}

		std::pair< future< void >, async_io_op > Path::schedule(const async_io_op & req)
		{
			auto func = [this](){
				this->schedule();
			};
			return dispatcher->call(req, func);
		}

		std::pair< future< bool >, async_io_op > Path::add_handler(const async_io_op & req, Handler* h)
		{
			auto func = [this, h]() -> bool {
				return this->add_handler(h);
			};
			return dispatcher->call(req, func);
		}

		std::pair< future< bool >, async_io_op > Path::remove_handler(const async_io_op & req, Handler* h)
		{
			auto func = [this, h]() -> bool {
				return this->remove_handler(h);
			};
			return dispatcher->call(req, func);
		}


		bool Path::remove_ent(std::unordered_map<directory_entry, directory_entry>& dict,const directory_entry& ent)
		{
			BOOST_AFIO_SPIN_LOCK_GUARD lk(sp_lock);
			try
			{
				if(0 < dict.erase(ent) )
				{	
					//std::cout << "remove_ent was successful\n";
					return true;
				}
				else 
				{
					std::cout << "the ent could not be removed\n";
					return false;
				}
			}
			catch(std::exception &e)//this should probably be more specific
			{
				std::cout << "remove_ent had an error\n" << e.what()<< std::endl;
				//throw;//should this throw???
				return false;
			}
		}

		bool Path::add_ent(std::unordered_map<directory_entry, directory_entry>& dict, const directory_entry& ent)
		{
			// libstdc++ doesn't come with std::lock_guard
			BOOST_AFIO_SPIN_LOCK_GUARD lk(sp_lock);
			try
			{
				if(dict.insert(std::make_pair(ent, ent)).second)
					return true;
				else
					return false;
			}
			catch(std::exception& e)
			{
				std::cout << "error inserting " << ent.name() 
					<< " into the hash_table: "<< e.what()<<std::endl;
				return false;
			}
		}

		void Path::schedule()
		{
			//BOOST_AFIO_LOCK_GUARD<boost::mutex> lk(mtx);
			auto t_source(dispatcher->threadsource().lock());
			
			//why don't the other timers work????
			//boost::asio::high_resolution_timer t(t_source->io_service(), milli_sec(500));

			timer = std::make_shared<boost::asio::deadline_timer>(t_source->io_service(), milli_sec(poll_rate));
			std::weak_ptr<boost::asio::deadline_timer> wk_timer = timer;
			timer->async_wait(std::bind(&Path::collect_data, this, wk_timer));
			//std::cout << "Setup the async callback\n";
			dispatcher->call(async_io_op(), [t_source](){t_source->io_service().run();});
			//std::cout <<"setup was successful\n";
			//timers.push_back(timer);
		}

		void Path::collect_data(std::weak_ptr<boost::asio::deadline_timer> t)
		{
			if(std::filesystem::exists(name))
			{
				BOOST_AFIO_LOCK_GUARD<boost::mutex> lk(mtx);
				
				auto dir(dispatcher->dir(boost::afio::async_path_op_req(name)));
				
	    		std::pair<std::vector<boost::afio::directory_entry>, bool> list;
	    		async_io_op my_op;
			    
			    // This is used to reset the enumeration to the start
			    bool restart=true;
			    do{				        
			        auto enumerate(dispatcher->enumerate(boost::afio::async_enumerate_op_req(dir,boost::afio::directory_entry::compatibility_maximum())));
			        restart=false;
			        list=enumerate.first.get();			        
			        my_op = enumerate.second;
			    } while(list.second);
		
			    auto new_ents = std::make_shared<std::vector<directory_entry>>(std::move(list.first));

			    dispatcher->call(async_io_op(), std::bind(&Path::monitor, this, *ents_ptr, *new_ents, my_op.h->get()));
			    ents_ptr = std::move(new_ents);

			    if(auto atimer = t.lock())
			    {
			        //t->expires_at(t->expires_at() + milli_sec(poll_rate) );
			        atimer->expires_from_now( milli_sec(poll_rate) );
			    	atimer->async_wait(std::bind(&Path::collect_data, this, t));
			    }
			}
			std::cout << "data collected\n";
		}

		//void Path::monitor(boost::asio::high_resolution_timer* t)
		void Path::monitor(std::vector<directory_entry> old_ents, std::vector<directory_entry> new_ents, std::shared_ptr<async_io_handle> dirh)
		{
			std::cout << "Monitor was caled\n";
			std::unordered_map<directory_entry, directory_entry> dict;
			std::vector<std::function<bool()>> move_funcs;
		    std::vector<async_io_op> move_ops;
		    move_funcs.reserve(old_ents.size());
		    move_ops.reserve(old_ents.size());
		    BOOST_FOREACH(auto &i, old_ents)
		    {
		    	move_ops.push_back(async_io_op());
		      	move_funcs.push_back([this, &i, &dict](){return this->add_ent(dict, i);});
		      	//add_ent(i);
		    }		      		
		    			    
		    auto make_dict(dispatcher->call(move_ops, move_funcs)); 
		    //auto fut = &remake_dict.first;
		    when_all(make_dict.first.begin(), make_dict.first.end()).wait();
		

		    
		    //std::vector<async_io_op> ops;
			//ops.reserve(old_ents->size());
			std::vector<std::function<directory_entry()>> old_stat;
			old_stat.reserve(old_ents.size());

		    BOOST_FOREACH( auto &i, old_ents)
		    {
		    	old_stat.push_back( [&i, dirh]()->directory_entry&{ i.fetch_lstat(dirh); return i; } );
				//ops.push_back(async_io_op());
		    }

		    auto old_stat_ents(dispatcher->call(make_dict.second, old_stat));

		    std::vector<std::function<directory_entry()>> new_stat;
			new_stat.reserve(new_ents.size());

		    BOOST_FOREACH( auto &i, new_ents)
		    {
		    	new_stat.push_back( [&i, dirh]()->directory_entry&{ i.fetch_lstat(dirh); return i; } );
				//ops.push_back(async_io_op());
		    }
			
			auto new_stat_ents(dispatcher->call(old_stat_ents.second, new_stat));
		    //when_all(stat_ents.first.begin(), stat_ents.first.end()).wait();
		    
		    std::vector<std::function<bool()>> comp_funcs;
	    	comp_funcs.reserve(new_ents.size());
	    	//std::pair<std::vector<future<bool>>, std::vector<async_io_op>> compare;
	    	
	    	BOOST_FOREACH(auto &i, new_stat_ents.first)
	    	{    //i.wait();
	    		auto ptr = &i;
	    		comp_funcs.push_back(std::bind(
	    			[this, dirh, &dict](future<directory_entry>* fut)->bool{
	    			return this->compare_entries(dict, std::move(*fut), dirh);		    			
	    		}, 
	    		ptr));
	    	}
	    
		    auto compare(dispatcher->call(new_stat_ents.second, comp_funcs));
		    when_all(compare.second.begin(), compare.second.end()).wait();

		   // auto comp_barrier(dispatcher->barrier(compare.second));
		   
		    // there is a better way to schedule this but I'm missing it. 
		    // I want dict to hold only the deleted files, but I need to schedule
		    // the next op in the lambda, and then I don't know how to 
		    // finish the rest
		    
		    std::vector<std::function<void()>> clean_funcs;
			std::vector<async_io_op> clean_ops;
	
		    auto setup_removal(dispatcher->call(async_io_op(), [&](){

				clean_ops.reserve(dict.size());
		    	clean_funcs.reserve(dict.size());
		    	BOOST_FOREACH(auto &i, dict)
		    	{
		    		clean_ops.push_back(async_io_op());
		    		clean_funcs.push_back([this, &i](){
		    			 return this->clean(i.second);
		    		});
		    	}
		    }));
	    	
	    	
	    	setup_removal.first.wait();

		    auto clean_dict(dispatcher->call(clean_ops, clean_funcs)); 

		    //when_all(clean_dict.first.begin(), clean_dict.first.end()).wait();
	
		}// end monitor()

		void Path::clean(directory_entry& ent)
		{
			//try{
			//std::cout << "Cleaning the dict... \n";
			// anything left in dict has been deleted
			dir_event event;
			event.eventNo=++(*eventcounter);
			event.setDeleted();
			event.path = ent.name();
			BOOST_FOREACH(auto i, handlers)
				dispatcher->call(async_io_op(), [i, event](){ i.second(event); });

		}

		bool Path::compare_entries(std::unordered_map<directory_entry, directory_entry>& dict, future<directory_entry> fut, std::shared_ptr< async_io_handle> dirh)
		{
			//std::cout << "comparing entries... \n";
			//static std::atomic<int> eventcounter(0);
			std::shared_ptr<dir_event> ret;
			auto entry = fut.get(); //}catch(std::exception &e){std::cout << "Getting this future cuased and error\n" << e.what() <<std::endl;}
			
						
				// try to find the directory_entry
				// if it exists, then determine if anything has changed
				// if the hash is successful then it has the same inode and ctim
			bool new_file = false;
			directory_entry* temp = nullptr;

			{
				BOOST_AFIO_SPIN_LOCK_GUARD lk(sp_lock);
				{
					auto it = dict.find(entry);
					if(it == dict.end())
						new_file = true;
					else
						temp = &(it->second);
				}
			}

			if(new_file)
			{
				//std::cout << "we have a new file: "  << entry.name() <<std::endl;
				//We've never seen this before
				ret = std::make_shared<dir_event>();
				ret->eventNo=++(*eventcounter);
				ret->path = entry.name();
				ret->setCreated();
			}
			else	
			{		
			// consider replacing this whole section with XORs and using a metadata_flags
			// to track the changes to the files, will require changing dir_event
				bool changed = false;
				bool renamed = false;
				bool modified = false;
				bool security = false;
				
				if(temp->name() != entry.name())
				{
					changed = true;
					renamed = true;
				}

				if(temp->st_mtim()!= entry.st_mtim()
						|| temp->st_ctim() != entry.st_ctim()
						|| temp->st_size() != entry.st_size() 
						|| temp->st_allocated() != entry.st_allocated())
				{
					modified = true;
					changed = true;
				}

				if(temp->st_type() != entry.st_type()
		#ifndef WIN32
						|| temp->st_mode() != entry.st_mode()
		#endif
						)
				{
					changed = true;
					security = true;
				}
		
				if(changed)
				{
					//std::cout << "Found a changed entry!\n";
					ret = std::make_shared<dir_event>();
					ret->eventNo = ++(*eventcounter);
					ret->path = entry.name();
					ret->setRenamed(renamed);
					ret->setModified(modified);
					ret->setSecurity(security);
				}

				
				remove_ent(dict, *temp);
			}//end if
			
			if(ret)
			{
				//std::cout << "Scheduling handlers...\n";
				auto event = *ret;
				BOOST_FOREACH(auto &i, handlers)
					dispatcher->call(async_io_op(), [i, event](){ i.second(event); });
			}
			return true;			
		}// end compare_entries

		bool Path::add_handler(Handler* h)
		{
			BOOST_AFIO_SPIN_LOCK_GUARD lk(sp_lock);
			try
			{
				//std::cout << "The handler address is: " << h << std::endl;
				auto temp = *h;
				if(handlers.insert(std::make_pair(h, std::move(temp))).second)
				{
					//std::cout << "The handler was addedd\n";
					return true;
				}
				else
				{
					//std::cout << "Error adding the Handler!!!!\n";
					return false;
				}
			}
			catch(std::exception& e)
			{
				//std::cout << "error inserting the handler into the hash_table: "<< e.what()<<std::endl;
				return false;
			}
		}

		bool Path::remove_handler(Handler* h)
		{
			//cant figure out how to do this better than with a try-catch...
			//BOOST_AFIO_SPIN_LOCK_GUARD lk(sp_lock);
			try{
			
				//std::cout << "The handler address is: " << h << std::endl;
				
				auto temp = handlers.size();
				//std::cout << "The size of handlers is "<< temp << std::endl;
				if(handlers.erase(h) > 0)
					return true;
				else
				{
					//std::cout << "The size of handlers is "<< temp << std::endl;
					if(handlers.size() < temp)
						return true;
					return false;
				}
			}
			catch(std::exception& e)
			{
				std::cout << "error removing a handler from the hash_table: "<< e.what() << std::endl;
				return false;
			}
		}
	}// namespace afio
}//namespace boost