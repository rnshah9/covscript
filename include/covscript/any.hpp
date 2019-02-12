#pragma once
/*
* Covariant Script Any
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* Copyright (C) 2019 Michael Lee(李登淳)
* Email: mikecovlee@163.com
* Github: https://github.com/mikecovlee
*/
#include <covscript/mozart/memory.hpp>

namespace cs_impl {
// Be careful when you adjust the buffer size.
	constexpr std::size_t default_allocate_buffer_size = 64;
	template<typename T> using default_allocator_provider=std::allocator<T>;

	class any final {
		class baseHolder {
		public:
			baseHolder() = default;

			virtual ~ baseHolder() = default;

			virtual const std::type_info &type() const = 0;

			virtual baseHolder *duplicate() = 0;

			virtual bool compare(const baseHolder *) const = 0;

			virtual long to_integer() const = 0;

			virtual std::string to_string() const = 0;

			virtual std::size_t hash() const = 0;

			virtual void detach() = 0;

			virtual void kill() = 0;

			virtual cs::namespace_t &get_ext() const = 0;

			virtual const char *get_type_name() const = 0;
		};

		template<typename T>
		class holder : public baseHolder {
		protected:
			T mDat;
		public:
			static cov::allocator<holder<T>, default_allocate_buffer_size, default_allocator_provider> allocator;

			holder() = default;

			template<typename...ArgsT>
			explicit holder(ArgsT &&...args):mDat(std::forward<ArgsT>(args)...) {}

			~ holder() override = default;

			const std::type_info &type() const override
			{
				return typeid(T);
			}

			baseHolder *duplicate() override
			{
				return allocator.alloc(mDat);
			}

			bool compare(const baseHolder *obj) const override
			{
				if (obj->type() == this->type())
					return cs_impl::compare(mDat, static_cast<const holder<T> *>(obj)->data());
				else
					return false;
			}

			long to_integer() const override
			{
				return cs_impl::to_integer(mDat);
			}

			std::string to_string() const override
			{
				return cs_impl::to_string(mDat);
			}

			std::size_t hash() const override
			{
				return cs_impl::hash<T>(mDat);
			}

			void detach() override
			{
				cs_impl::detach(mDat);
			}

			void kill() override
			{
				allocator.free(this);
			}

			virtual cs::namespace_t &get_ext() const override
			{
				return cs_impl::get_ext<T>();
			}

			const char *get_type_name() const override
			{
				return cs_impl::get_name_of_type<T>();
			}

			T &data()
			{
				return mDat;
			}

			const T &data() const
			{
				return mDat;
			}

			void data(const T &dat)
			{
				mDat = dat;
			}
		};

		struct proxy {
			bool is_rvalue = false;
			short protect_level = 0;
			std::size_t refcount = 1;
			baseHolder *data = nullptr;

			proxy() = default;

			proxy(std::size_t rc, baseHolder *d) : refcount(rc), data(d) {}

			proxy(short pl, std::size_t rc, baseHolder *d) : protect_level(pl), refcount(rc), data(d) {}

			~proxy()
			{
				if (data != nullptr)
					data->kill();
			}
		};

		static cov::allocator<proxy, default_allocate_buffer_size, default_allocator_provider> allocator;
		proxy *mDat = nullptr;

		proxy *duplicate() const noexcept
		{
			if (mDat != nullptr) {
				++mDat->refcount;
			}
			return mDat;
		}

		void recycle() noexcept
		{
			if (mDat != nullptr) {
				--mDat->refcount;
				if (mDat->refcount == 0) {
					allocator.free(mDat);
					mDat = nullptr;
				}
			}
		}

		explicit any(proxy *dat) : mDat(dat) {}

	public:
		void swap(any &obj, bool raw = false)
		{
			if (this->mDat != nullptr && obj.mDat != nullptr && raw) {
				if (this->mDat->protect_level > 0 || obj.mDat->protect_level > 0)
					throw cov::error("E000J");
				baseHolder *tmp = this->mDat->data;
				this->mDat->data = obj.mDat->data;
				obj.mDat->data = tmp;
			}
			else {
				proxy *tmp = this->mDat;
				this->mDat = obj.mDat;
				obj.mDat = tmp;
			}
		}

		void swap(any &&obj, bool raw = false)
		{
			if (this->mDat != nullptr && obj.mDat != nullptr && raw) {
				if (this->mDat->protect_level > 0 || obj.mDat->protect_level > 0)
					throw cov::error("E000J");
				baseHolder *tmp = this->mDat->data;
				this->mDat->data = obj.mDat->data;
				obj.mDat->data = tmp;
			}
			else {
				proxy *tmp = this->mDat;
				this->mDat = obj.mDat;
				obj.mDat = tmp;
			}
		}

		void clone()
		{
			if (mDat != nullptr) {
				if (mDat->protect_level > 2)
					throw cov::error("E000L");
				proxy *dat = allocator.alloc(1, mDat->data->duplicate());
				recycle();
				mDat = dat;
			}
		}

		void try_move() const
		{
			if (mDat != nullptr && mDat->refcount == 1) {
				mDat->protect_level = 0;
				mDat->is_rvalue = true;
			}
		}

		bool usable() const noexcept
		{
			return mDat != nullptr;
		}

		template<typename T, typename...ArgsT>
		static any make(ArgsT &&...args)
		{
			return any(allocator.alloc(1, holder<T>::allocator.alloc(std::forward<ArgsT>(args)...)));
		}

		template<typename T, typename...ArgsT>
		static any make_protect(ArgsT &&...args)
		{
			return any(allocator.alloc(1, 1, holder<T>::allocator.alloc(std::forward<ArgsT>(args)...)));
		}

		template<typename T, typename...ArgsT>
		static any make_constant(ArgsT &&...args)
		{
			return any(allocator.alloc(2, 1, holder<T>::allocator.alloc(std::forward<ArgsT>(args)...)));
		}

		template<typename T, typename...ArgsT>
		static any make_single(ArgsT &&...args)
		{
			return any(allocator.alloc(3, 1, holder<T>::allocator.alloc(std::forward<ArgsT>(args)...)));
		}

		constexpr any() = default;

		template<typename T>
		any(const T &dat):mDat(allocator.alloc(1, holder<T>::allocator.alloc(dat))) {}

		any(const any &v) : mDat(v.duplicate()) {}

		any(any &&v) noexcept
		{
			swap(std::forward<any>(v));
		}

		~any()
		{
			recycle();
		}

		const std::type_info &type() const
		{
			return this->mDat != nullptr ? this->mDat->data->type() : typeid(void);
		}

		long to_integer() const
		{
			if (this->mDat == nullptr)
				return 0;
			return this->mDat->data->to_integer();
		}

		std::string to_string() const
		{
			if (this->mDat == nullptr)
				return "Null";
			return this->mDat->data->to_string();
		}

		std::size_t hash() const
		{
			if (this->mDat == nullptr)
				return cs_impl::hash<void *>(nullptr);
			return this->mDat->data->hash();
		}

		void detach() const
		{
			if (this->mDat != nullptr) {
				if (this->mDat->protect_level > 2)
					throw cov::error("E000L");
				this->mDat->data->detach();
			}
		}

		cs::namespace_t &get_ext() const
		{
			if (this->mDat == nullptr)
				throw cs::runtime_error("Target type does not support extensions.");
			return this->mDat->data->get_ext();
		}

		std::string get_type_name() const
		{
			if (this->mDat == nullptr)
				return cxx_demangle(get_name_of_type<void>());
			else
				return cxx_demangle(this->mDat->data->get_type_name());
		}

		bool is_same(const any &obj) const
		{
			return this->mDat == obj.mDat;
		}

		bool is_rvalue() const
		{
			return this->mDat != nullptr && this->mDat->is_rvalue;
		}

		bool is_protect() const
		{
			return this->mDat != nullptr && this->mDat->protect_level > 0;
		}

		bool is_constant() const
		{
			return this->mDat != nullptr && this->mDat->protect_level > 1;
		}

		bool is_single() const
		{
			return this->mDat != nullptr && this->mDat->protect_level > 2;
		}

		void mark_as_rvalue(bool value) const
		{
			if (this->mDat != nullptr)
				this->mDat->is_rvalue = value;
		}

		void protect()
		{
			if (this->mDat != nullptr) {
				if (this->mDat->protect_level > 1)
					throw cov::error("E000G");
				this->mDat->protect_level = 1;
			}
		}

		void constant()
		{
			if (this->mDat != nullptr) {
				if (this->mDat->protect_level > 2)
					throw cov::error("E000G");
				this->mDat->protect_level = 2;
			}
		}

		void single()
		{
			if (this->mDat != nullptr) {
				if (this->mDat->protect_level > 3)
					throw cov::error("E000G");
				this->mDat->protect_level = 3;
			}
		}

		any &operator=(const any &var)
		{
			if (&var != this) {
				recycle();
				mDat = var.duplicate();
			}
			return *this;
		}

		any &operator=(any &&var) noexcept
		{
			if (&var != this)
				swap(std::forward<any>(var));
			return *this;
		}

		bool compare(const any &var) const
		{
			return usable() && var.usable() ? this->mDat->data->compare(var.mDat->data) : !usable() && !var.usable();
		}

		bool operator==(const any &var) const
		{
			return this->compare(var);
		}

		bool operator!=(const any &var) const
		{
			return !this->compare(var);
		}

		template<typename T>
		T &val(bool raw = false)
		{
			if (typeid(T) != this->type())
				throw cov::error("E0006");
			if (this->mDat == nullptr)
				throw cov::error("E0005");
			if (this->mDat->protect_level > 1)
				throw cov::error("E000K");
			if (!raw)
				clone();
			return static_cast<holder<T> *>(this->mDat->data)->data();
		}

		template<typename T>
		const T &val(bool raw = false) const
		{
			if (typeid(T) != this->type())
				throw cov::error("E0006");
			if (this->mDat == nullptr)
				throw cov::error("E0005");
			return static_cast<const holder<T> *>(this->mDat->data)->data();
		}

		template<typename T>
		const T &const_val() const
		{
			if (typeid(T) != this->type())
				throw cov::error("E0006");
			if (this->mDat == nullptr)
				throw cov::error("E0005");
			return static_cast<const holder<T> *>(this->mDat->data)->data();
		}

		template<typename T>
		explicit operator const T &() const
		{
			return this->const_val<T>();
		}

		void assign(const any &obj, bool raw = false)
		{
			if (&obj != this && obj.mDat != mDat) {
				if (mDat != nullptr && obj.mDat != nullptr && raw) {
					if (this->mDat->protect_level > 0 || obj.mDat->protect_level > 0)
						throw cov::error("E000J");
					mDat->data->kill();
					mDat->data = obj.mDat->data->duplicate();
				}
				else {
					recycle();
					if (obj.mDat != nullptr)
						mDat = allocator.alloc(1, obj.mDat->data->duplicate());
					else
						mDat = nullptr;
				}
			}
		}

		template<typename T>
		void assign(const T &dat, bool raw = false)
		{
			if (mDat != nullptr && raw) {
				if (this->mDat->protect_level > 0)
					throw cov::error("E000J");
				mDat->data->kill();
				mDat->data = holder<T>::allocator.alloc(dat);
			}
			else {
				recycle();
				mDat = allocator.alloc(1, holder<T>::allocator.alloc(dat));
			}
		}

		template<typename T>
		any &operator=(const T &dat)
		{
			assign(dat);
			return *this;
		}
	};

	template<>
	std::string to_string<std::string>(const std::string &str)
	{
		return str;
	}

	template<>
	std::string to_string<bool>(const bool &v)
	{
		if (v)
			return "true";
		else
			return "false";
	}

	template<int N>
	class any::holder<char[N]> : public any::holder<std::string> {
	public:
		using holder<std::string>::holder;
	};

	template<>
	class any::holder<std::type_info> : public any::holder<std::type_index> {
	public:
		using holder<std::type_index>::holder;
	};

	template<typename T> cov::allocator<any::holder<T>, default_allocate_buffer_size, default_allocator_provider> any::holder<T>::allocator;
}

std::ostream &operator<<(std::ostream &, const cs_impl::any &);

namespace std {
	template<>
	struct hash<cs_impl::any> {
		std::size_t operator()(const cs_impl::any &val) const
		{
			return val.hash();
		}
	};
}
