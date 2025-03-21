#include "audio_manager.h"
#include <condition_variable>
#include "util.h"


// 饿汉模式,避免多线程时instance()线程不安全,在main之前初始化,还没有多线程
AudioManager* AudioManager::manager = new AudioManager;

AudioManager::AudioManager()
{
	audio_player = new std::thread([&]()
		{
			while (true)
			{
				{
					// 为了避免性能消耗,加入同步互斥机制
					// 只用在交换缓冲区加锁就行
					std::unique_lock<std::mutex> lock(mtx);
					cond.wait(lock);

					if (load_queue.empty() && !load_queue_buffer.empty())
						load_queue.swap(load_queue_buffer);

					if (play_queue.empty() && !play_queue_buffer.empty())
						play_queue.swap(play_queue_buffer);

					if (stop_queue.empty() && !stop_queue_buffer.empty())
						stop_queue.swap(stop_queue_buffer);

					if (resume_queue.empty() && !resume_queue_buffer.empty())
						resume_queue.swap(resume_queue_buffer);

					if (pause_queue.empty() && !pause_queue_buffer.empty())
						pause_queue.swap(pause_queue_buffer);
				}


				while (!load_queue.empty())
				{
					auto front = load_queue.front();
					load_queue.pop();
					load_audio(front.path.c_str(), front.id.c_str());
				}

				while (!play_queue.empty())
				{
					auto front = play_queue.front();
					play_queue.pop();
					play_audio(front.id.c_str(), front.is_loop);
				}

				while (!stop_queue.empty())
				{
					auto front = stop_queue.front();
					stop_queue.pop();
					stop_audio(front.c_str());
				}

				while (!resume_queue.empty())
				{
					auto front = resume_queue.front();
					resume_queue.pop();
					resume_audio(front.c_str());
				}

				while (!pause_queue.empty())
				{
					auto front = pause_queue.front();
					pause_queue.pop();
					pause_audio(front.c_str());
				}
			}
		});
}

// 单例生命周期随进程,析构不会调用
AudioManager::~AudioManager()
{
	if (audio_player)
	{
		audio_player->join();
		delete audio_player;
	}
}

AudioManager* AudioManager::instance()
{
	return manager;
}

// 这里堵塞式申请锁会影响游戏, todo:无锁队列
// 但是player线程大多数时候阻塞等待,就主线程频繁申请锁,有必要吗?
// 或许,播放时不加锁也行,丢失音频总比游戏卡顿好
void AudioManager::load_audio_ex(const std::wstring path, const std::wstring id)
{
	{
		std::unique_lock<std::mutex> lock(mtx);
		load_queue_buffer.push({ path, id });
	}

	cond.notify_one();
}

void AudioManager::play_audio_ex(const std::wstring id, bool is_loop)
{
	{
		std::unique_lock<std::mutex> lock(mtx);
		play_queue_buffer.push({ id, is_loop });
	}

	cond.notify_one();
}

void AudioManager::stop_audio_ex(const std::wstring id)
{
	{
		std::unique_lock<std::mutex> lock(mtx);
		stop_queue_buffer.push(id);
	}

	cond.notify_one();
}

void AudioManager::pause_audio_ex(const std::wstring id)
{
	{
		std::unique_lock<std::mutex> lock(mtx);
		pause_queue_buffer.push(id);
	}

	cond.notify_one();
}

void AudioManager::resume_audio_ex(const std::wstring id)
{
	{
		std::unique_lock<std::mutex> lock(mtx);
		resume_queue_buffer.push(id);
	}

	cond.notify_one();
}
